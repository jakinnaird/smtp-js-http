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

#include <vector>

static void my_fatal(void *udata, const char *msg)
{
    (void)udata;
    throw std::runtime_error(msg);
}

ScriptVM::ScriptVM(const std::string &scriptPath)
    : m_ScriptPath(scriptPath)
{
    spdlog::debug("Creating javscript environment");

    m_VM = duk_create_heap(nullptr, nullptr, nullptr, nullptr, my_fatal);
    if (m_VM == nullptr)
        throw std::runtime_error("Failed to create VM heap");

    // register all script classes
    dukglue_register_constructor<WebRequest>(m_VM, "WebRequest");
    dukglue_register_method(m_VM, &WebRequest::Test, "Test");
}

ScriptVM::~ScriptVM(void)
{
    if (m_VM)
        duk_destroy_heap(m_VM);

    spdlog::debug("Destroying javascript environment");
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

        spdlog::debug("Calling method {} in file {}", method.c_str(), script.c_str());
    }
}
