// smtp-js-http

// Copyright (c) 2020 James Kinnaird
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "spdlog/spdlog.h"

#include "smtp.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>

void* get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendLine(int sock, const std::string &line)
{
    int total = 0;
    int bytesLeft = line.length() + 1;
    int n;

    std::string _line(line);
    _line.append("\r\n");
    const char *buf = _line.c_str();

    while (total < line.length() + 1)
    {
        int n = send(sock, buf + total, bytesLeft, 0);
        if (n == -1)
            break;

        total += n;
        bytesLeft -= n;
    }

    return (n == -1 ? -1 : (total - 1));
}

int readLine(int sock, std::string &line)
{
    int retval = -1;
    char ch = 0;
    int n;

    do
    {
        retval = recv(sock, &ch, sizeof(ch), 0);
        if (retval <= 0)
            break;

        if (ch != '\r' && ch != '\n')
            line.push_back(ch);
    } while (ch != '\n');

    return retval;
}

class SMTPConn
{
private:
    int m_Socket;
    moodycamel::ConcurrentQueue<email> &m_Queue;

    enum State
    {
        STATE_CONNECTION,
        STATE_EHLO,
        STATE_COMMANDS,
        STATE_DATA,
        STATE_DATAEOM,
    };

    State m_State;

    std::string m_Client;

    email m_Mail;

public:
    SMTPConn(int sock, moodycamel::ConcurrentQueue<email> &queue)
        : m_Socket(sock), m_Queue(queue)
    {
        spdlog::debug("Creating new SMTP connection for socket {}", m_Socket);

        m_State = STATE_CONNECTION;
    }

    ~SMTPConn(void)
    {
        spdlog::debug("Destroying SMTP connection for socket {}", m_Socket);
    }

    int Update(void)
    {
        int result = -1;

        switch (m_State)
        {
            case STATE_CONNECTION:
            {
                // it's a new connection. send the server id line
                std::ostringstream oss;
                oss << "220 smtp-js-http Ready";

                result = sendLine(m_Socket, oss.str());
                if (result == oss.str().length())
                    m_State = STATE_EHLO;
                else
                    spdlog::warn("SMTP server: failed to send greeting to client {}", m_Socket);
            } break;

            case STATE_EHLO:
            {
                std::string ehlo;
                result = readLine(m_Socket, ehlo);
                if (result > 0) 
                {
                    // check if the client sent a EHLO
                    if (ehlo.find("EHLO") != std::string::npos ||
                        ehlo.find("HELO") != std::string::npos)
                    {
                        m_Client = ehlo.substr(5);
                        spdlog::debug("SMTP server client {} name is {}", m_Socket, m_Client.c_str());

                        // currently we don't support any options
                        std::ostringstream oss;
                        oss << "250 smtp-js-http greets " << m_Client;
                        result = sendLine(m_Socket, oss.str());
                        if (result == oss.str().length())
                            m_State = STATE_COMMANDS;
                        else
                        {
                            spdlog::warn("SMTP server: failed to send options to client {}", m_Socket);
                            result = 0;
                        }
                    }
                    else
                        spdlog::debug("SMTP server: client {} didn't send EHLO or HELO", m_Socket);
                }
            } break;

            case STATE_COMMANDS:
            {
                std::string line;
                result = readLine(m_Socket, line);
                if (result > 0)
                {
                    if (line.find("MAIL FROM:") != std::string::npos)
                    {
                        m_Mail.from.assign(line.substr(11, line.length() - 12));
                        spdlog::debug("SMTP server: client {} sending email from {}", m_Socket, m_Mail.from.c_str());
                    }
                    else if (line.find("RCPT TO:") != std::string::npos)
                    {
                        std::string to(line.substr(9, line.length() - 10));
                        m_Mail.to.push_back(to);
                        spdlog::debug("SMTP server: client {} sending email to {}", m_Socket, to.c_str());
                    }
                    else if (line.find("DATA") != std::string::npos)
                    {
                        std::string cmd("354 Start mail input; end with <CRLF>.<CRLF>");
                        if (sendLine(m_Socket, cmd) == cmd.length())
                            m_State = STATE_DATA;                        
                    }
                    else if (line.compare("QUIT") == 0)
                        result = 0;
                }
            } break;

            case STATE_DATA:
            {
                std::string line;
                result = readLine(m_Socket, line);
                if (result > 0)
                {
                    if (line.empty() || line.compare("\\r\\n") == 0)
                        m_State = STATE_DATAEOM;
                    else
                    {
                        spdlog::debug("Processing line from client {}: {}", m_Socket, line.c_str());

                        // process the line
                        if (line.find("Date:") == 0)
                            m_Mail.date.assign(line.substr(6));
                        else if (line.find("Subject:") == 0)
                            m_Mail.subject.assign(line.substr(9));
                        else if (line.find("From:") == 0)
                            ;   // we don't care
                        else if (line.find("To:") == 0)
                            ; // we don't care
                        else
                        {
                            if (m_Mail.body.empty())
                                m_Mail.body.assign(line);
                            else
                                m_Mail.body.append(line);

                            // if (line.length() > 0)
                            //     m_Mail.body.append("\r\n");
                        }
                    }
                }
            } break;

            case STATE_DATAEOM:
            {
                std::string line;
                result = readLine(m_Socket, line);
                if (result > 0)
                {
                    spdlog::debug("Processing line for EOM: {}", line.c_str());

                    if (line.compare(".") == 0)
                    {
                        if (sendLine(m_Socket, "250 OK") == 6)
                            m_State = STATE_COMMANDS;

                        spdlog::debug("SMTP server: Enqueuing mail from client {}", m_Socket);

                        // queue the message
                        m_Queue.enqueue(m_Mail);

                        // clear up the mail packet
                        m_Mail.from.clear();
                        m_Mail.to.clear();
                        m_Mail.date.clear();
                        m_Mail.subject.clear();
                        m_Mail.body.clear();
                    }
                    else
                    {
                        m_Mail.body.append(line);
                        m_Mail.body.append("\r\n");
                        m_State = STATE_DATA;
                    }
                }

            } break;

            default: break;
        }

        return result;
    }
};

SMTPServer::SMTPServer(moodycamel::ConcurrentQueue<email> &queue)
    : m_Queue(queue)
{
    m_Listener = -1;
    FD_ZERO(&m_Master);
    m_FdMax = 0;
}

SMTPServer::~SMTPServer(void)
{
}

bool SMTPServer::Start(const std::string &addr, const std::string &port)
{
    int yes = 1;
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int retval = getaddrinfo(addr.c_str(), port.c_str(), &hints, &ai);
    if (retval != 0)
    {
        spdlog::error("SMTP server start failed: {}",
            gai_strerror(retval));
        return false;
    }

    for (p = ai; p != nullptr; p = p->ai_next)
    {
        m_Listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_Listener < 0)
            continue;

        setsockopt(m_Listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(m_Listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(m_Listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    if (p == nullptr)
    {
        spdlog::error("SMTP server start failed: Unable to bind to {}:{}",
            addr.c_str(), port.c_str());
        return false;
    }

    if (listen(m_Listener, 10) == -1)
    {
        spdlog::error("SMTP server start failed: Unable to listen");
        return false;
    }

    FD_SET(m_Listener, &m_Master);
    m_FdMax = m_Listener;

    spdlog::info("SMTP server started on {}:{}",
        addr.c_str(), port.c_str());
    return true;
}

void SMTPServer::Stop(void)
{
    for (std::map<int, SMTPConn*>::iterator conn = m_Connections.begin();
        conn != m_Connections.end(); ++conn)
    {
        delete conn->second;
        close(conn->first);
        FD_CLR(conn->first, &m_Master);
    }
    m_Connections.clear();

    close(m_Listener);
    m_FdMax = -1;
}

bool SMTPServer::Update(void)
{
    fd_set read_fds = m_Master;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;

    if (m_FdMax == -1)
        return false;   // we are stopped

    int retval = select(m_FdMax + 1, &read_fds, nullptr, nullptr, &tv);
    if (m_FdMax > 0 && retval == -1)
    {
        spdlog::error("SMTP server update failed: Unable to check sockets");
        return false;
    }
    else if (retval > 0)
    {
        for (int i = 0; i <= m_FdMax; ++i)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == m_Listener)    // new connection
                {
                    struct sockaddr_storage remoteaddr;
                    socklen_t addrlen = sizeof(remoteaddr);
                    char remoteIP[INET6_ADDRSTRLEN];

                    int newfd = accept(m_Listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1)
                        spdlog::warn("SMTP server update failed: Unable to accept new connection");
                    else
                    {
                        spdlog::info("SMTP server: client {} connected from {}", newfd,
                            inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN));

                        SMTPConn *conn = new SMTPConn(newfd, m_Queue);
                        if (conn == nullptr)
                            spdlog::error("SMTP server failed to create connection");
                        else
                        {
                            if (conn->Update() > 0)
                            {
                                m_Connections[newfd] = conn;

                                FD_SET(newfd, &m_Master);
                                if (newfd > m_FdMax)
                                    m_FdMax = newfd;
                            }
                            else
                            {
                                delete conn;
                                close(newfd);
                            }
                        }
                    }
                }
                else    // existing connection
                {
                    std::map<int, SMTPConn*>::iterator conn = m_Connections.find(i);
                    if (conn != m_Connections.end() && conn->second)
                    {
                        int retval = conn->second->Update();
                        if (retval == 0)    // closing
                            sendLine(i, "221 smtp-js-http Service closing transmission channel");

                        if (retval == 0 || retval < 0)
                        {
                            spdlog::info("SMTP server: closing connection to client {}", conn->first);
                            close(conn->first);
                            FD_CLR(conn->first, &m_Master);

                            delete conn->second;
                            m_Connections.erase(conn);
                        }
                    }
                    else
                    {
                        spdlog::warn("SMTP server: Unhandled existing connection");
                        close(i);
                        FD_CLR(i, &m_Master);
                    }
                }
            }
        }
    }

    return true;
}
