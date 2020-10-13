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

#pragma once

#include <curl/curl.h>
#include <string>

class WebRequest
{
private:
    CURL *m_Curl;
    char m_errorBuf[CURL_ERROR_SIZE * 2];

    std::string m_PostData;
    std::string m_Error;
    std::string m_Result;

    struct curl_slist *m_Headers;

public:
    WebRequest(void);
    ~WebRequest(void);

    void Header(const std::string &name, const std::string &value);
    void PostData(const std::string &data);

    int32_t Get(const std::string &url);
    int32_t Post(const std::string &url);

    std::string Result(void) const { return m_Result; }
    std::string Error(void) const { return m_Error; }

private:
    int32_t Perform(const std::string &url);
};
