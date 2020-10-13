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

#include "scriptvm.hpp"
#include "dukglue/dukglue.h"
#include "spdlog/spdlog.h"
#include "webrequest.hpp"

#include <fstream>
#include <vector>

static void my_fatal(void *udata, const char *msg)
{
    (void)udata;
    throw std::runtime_error(msg);
}

static void my_info(const char *msg)
{
    spdlog::info(msg);
}

static void my_warn(const char *msg)
{
    spdlog::warn(msg);
}

static void my_error(const char *msg)
{
    spdlog::error(msg);
}

static void my_debug(const char *msg)
{
    spdlog::debug(msg);
}

class ScriptEmail
{
private:
    const email &m_Mail;

public:
    ScriptEmail(const email &mail) : m_Mail(mail) {}
    
    std::string getFrom(void) const { return m_Mail.from; }
    std::string getDate(void) const { return m_Mail.date; }
    std::string getSubject(void) const { return m_Mail.subject; }
    std::string getBody(void) const { return m_Mail.body; }
};

ScriptVM::ScriptVM(const std::string &scriptPath)
    : m_ScriptPath(scriptPath)
{
    spdlog::debug("Creating javscript environment");

    m_VM = duk_create_heap(nullptr, nullptr, nullptr, nullptr, my_fatal);
    if (m_VM == nullptr)
        throw std::runtime_error("Failed to create VM heap");

    // register all script functions
    dukglue_register_function(m_VM, my_info, "info");
    dukglue_register_function(m_VM, my_warn, "warn");
    dukglue_register_function(m_VM, my_error, "error");
    dukglue_register_function(m_VM, my_debug, "debug");

    // register all script classes
    dukglue_register_property(m_VM, &ScriptEmail::getFrom, nullptr, "from");
    dukglue_register_property(m_VM, &ScriptEmail::getDate, nullptr, "date");
    dukglue_register_property(m_VM, &ScriptEmail::getSubject, nullptr, "subject");
    dukglue_register_property(m_VM, &ScriptEmail::getBody, nullptr, "body");

    dukglue_register_constructor<WebRequest>(m_VM, "WebRequest");
    dukglue_register_method(m_VM, &WebRequest::Header, "header");
    dukglue_register_method(m_VM, &WebRequest::PostData, "data");
    dukglue_register_method(m_VM, &WebRequest::Get, "get");
    dukglue_register_method(m_VM, &WebRequest::Post, "post");
    dukglue_register_property(m_VM, &WebRequest::Result, nullptr, "result");
    dukglue_register_property(m_VM, &WebRequest::Error, nullptr, "error");
}

ScriptVM::~ScriptVM(void)
{
    spdlog::debug("Destroying javascript environment");

    if (m_VM)
        duk_destroy_heap(m_VM);
}

void ScriptVM::RunScript(const email &mail)
{
    for (std::vector<std::string>::const_iterator to = mail.to.begin();
        to != mail.to.end(); ++to)
    {
        std::string::size_type _at = (*to).find('@');
        if (_at == std::string::npos)
            continue;   // invalid

        std::string method = (*to).substr(0, _at);
        std::string script = (*to).substr(_at + 1);

        std::string _script(m_ScriptPath);
        _script.append(script);

        spdlog::info("Calling method '{}' in file {}", method.c_str(), _script.c_str());

        std::ifstream file(_script);
        if (!file.is_open())
            spdlog::warn("Failed to open script file '{}'", _script.c_str());
        else
        {
            std::string content;
            std::string line;
            while (std::getline(file, line))
            {
                content.append(line);
                content.append("\n");
            }

            if (content.length() > 0)
            {
                duk_push_lstring(m_VM, content.c_str(), content.length());
                if (duk_peval(m_VM) != 0)
                {
                    spdlog::warn("Failed to compile script '{}': {}",
                        _script.c_str(), duk_safe_to_string(m_VM, -1));
                }
                else
                {
                    duk_pop(m_VM);

                    duk_push_global_object(m_VM);
                    duk_get_prop_string(m_VM, -1, method.c_str());
                    ScriptEmail *smail = new ScriptEmail(mail);
                    dukglue_push(m_VM, smail);
                    
                    duk_int_t result = duk_pcall(m_VM, 1);
                    if (result != DUK_EXEC_SUCCESS)
                    {
                        spdlog::warn("Failed to execute script '{}': {}",
                            _script.c_str(), duk_safe_to_string(m_VM, -1));
                    }

                    duk_pop(m_VM);
                    delete smail;
                }
            }
            else
            {
                spdlog::warn("Failed to read script file '{}'", _script.c_str());
            }
            
        }
    }
}
