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
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/syslog_sink.h"
#include "tclap/CmdLine.h"

#include <atomic>
#include <chrono>
#include <signal.h>
#include <systemd/sd-daemon.h>
#include <thread>

#include "scriptvm.hpp"
#include "smtp.hpp"

#define DEFAULT_CONF_PATH       "/etc/smtp-js-http"
#define DEFAULT_SMTP_ADDR       "127.0.0.1"
#define DEFAULT_SMTP_PORT       "25"
#define DEFAULT_SCRIPT_PATH     "/usr/share/smtp-js-http"
#define DEFAULT_LOG_FILE        "syslog"
#define DEFAULT_LOG_LEVEL       "info"

std::atomic_bool g_Running(false);

void signal_handler(int signo)
{
    switch (signo)
    {
    case SIGINT:
    case SIGTERM:
        if (signo == SIGINT)
            spdlog::info("SIGINT caught");
        else
            spdlog::info("SIGTERM caught");

        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        g_Running.store(false);
        
        break;
    }
}

void ThreadProc(const std::string &scriptPath, const moodycamel::ConcurrentQueue<email> &queue)
{
    try
    {
        moodycamel::ConcurrentQueue<email> &mailqueue = const_cast<moodycamel::ConcurrentQueue<email> &>(queue);
        ScriptVM *vm = new ScriptVM(scriptPath);

        while (g_Running)
        {
            email mail;
            if (mailqueue.try_dequeue(mail))
            {
                spdlog::debug("Processing email to {}", mail.to.front().c_str());
            }
            else
                std::this_thread::yield();
        }

        delete vm;
    }
    catch (std::exception &e)
    {
        spdlog::warn("Exception in thread: {}", e.what());
        g_Running.store(false);
    }
    catch (...)
    {
        g_Running.store(false);
    }
}

int main(int argc, char **argv)
{
    bool daemon = false;

    try
    {
        TCLAP::CmdLine args("smtp-js-http", ' ', "1.0", true);
        TCLAP::ValueArg<std::string> arg_conf("", "conf", "Path to config file. Default: " DEFAULT_CONF_PATH, false, DEFAULT_CONF_PATH, "path");
        TCLAP::SwitchArg arg_daemon("", "daemon", "Send notifications to systemd", false);
        TCLAP::ValueArg<std::string> arg_bindaddr("", "smtp-addr", "SMTP bind address. Default:" DEFAULT_SMTP_ADDR, false, DEFAULT_SMTP_ADDR, "smtp address");
        TCLAP::ValueArg<std::string> arg_port("", "smtp-port", "SMTP listening port. Default: " DEFAULT_SMTP_PORT, false, DEFAULT_SMTP_PORT, "smtp port");
        TCLAP::ValueArg<std::string> arg_script("", "script-path", "Path to JS files. Default: " DEFAULT_SCRIPT_PATH, false, DEFAULT_SCRIPT_PATH, "path");
        TCLAP::ValueArg<std::string> arg_logfile("", "log-path", "Path to log file. Default: " DEFAULT_LOG_FILE, false, DEFAULT_LOG_FILE, "path");
        TCLAP::ValueArg<std::string> arg_loglevel("", "log-level", "Active log level. Options: none, critical, error, warn, info, debug. Default: " DEFAULT_LOG_LEVEL, false, DEFAULT_LOG_LEVEL, "level");

        args.add(arg_loglevel);
        args.add(arg_logfile);
        args.add(arg_script);
        args.add(arg_port);
        args.add(arg_daemon);
        args.add(arg_conf);
        args.parse(argc, argv);

        daemon = arg_daemon.isSet();

        // configure the logger
        if (arg_logfile.getValue().compare("syslog") == 0)
            spdlog::set_default_logger(spdlog::syslog_logger_mt("syslog", "smtp-js-http", LOG_PID));
        else if (arg_logfile.getValue().compare("stdout") == 0)
            ; // nothing to do
        else
            spdlog::set_default_logger(spdlog::basic_logger_mt("smtp-js-http", arg_logfile.getValue()));

        // register our signal handlers
        if (signal(SIGTERM, signal_handler) == SIG_ERR ||
            signal(SIGINT, signal_handler) == SIG_ERR)
            throw std::runtime_error("Failed to install signal handler");

        if (arg_loglevel.getValue().compare("none") == 0)
            spdlog::set_level(spdlog::level::off);
        else if (arg_loglevel.getValue().compare("critical") == 0)
            spdlog::set_level(spdlog::level::critical);
        else if (arg_loglevel.getValue().compare("error") == 0)
            spdlog::set_level(spdlog::level::err);
        else if (arg_loglevel.getValue().compare("warn") == 0)
            spdlog::set_level(spdlog::level::warn);
        else if (arg_loglevel.getValue().compare("debug") == 0)
            spdlog::set_level(spdlog::level::debug);
        else
            spdlog::set_level(spdlog::level::info);

        spdlog::info("Starting smtp-js-http service");

        // ensure that there is a trailing slash
        std::string scriptPath(arg_script.getValue());
        if (scriptPath.rfind('/') != 0)
            scriptPath.append("/");

        spdlog::info("Using script path: {}", scriptPath.c_str());

        moodycamel::ConcurrentQueue<email> mailqueue;
        SMTPServer smtp(mailqueue);

        if (!smtp.Start(arg_bindaddr.getValue(), arg_port.getValue()))
            throw std::runtime_error("Unable to start SMTP server");

        if (daemon)
            sd_notify(0, "READY=1");

        g_Running.store(true);

        // start the script thread
        std::thread worker(ThreadProc, scriptPath, std::ref(mailqueue));

        // main loop
        while (g_Running)
        {
            if (!smtp.Update())
                g_Running.store(false);

            if (daemon)
                sd_notify(0, "WATCHDOG=1");
            
            std::this_thread::yield();
        }

        if (daemon)
            sd_notify(0, "STOPPING=1");

        spdlog::info("Stopping smtp-js-http service");

        smtp.Stop();
        worker.join();
    }
    catch (std::exception &e)
    {
        spdlog::critical(e.what());

        if (daemon)
            sd_notifyf(0, "STATUS=Failure: %s", e.what());
    }
    catch (...)
    {
        spdlog::critical("Unhandled fatal exception");

        if (daemon)
            sd_notify(0, "STATUS=Failure: Unhandled fatal exception");
    }

    spdlog::shutdown();
}
