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
#include "spdlog/spdlog.h"

static void my_fatal(void *udata, const char *msg)
{
    (void)udata;
    throw std::runtime_error(msg);
}

ScriptVM::ScriptVM(const std::string &scriptPath)
    : m_ScriptPath(scriptPath)
{
    m_VM = duk_create_heap(nullptr, nullptr, nullptr, nullptr, my_fatal);
    if (m_VM == nullptr)
        throw std::runtime_error("Failed to create VM heap");
}

ScriptVM::~ScriptVM(void)
{
    if (m_VM)
        duk_destroy_heap(m_VM);
}
