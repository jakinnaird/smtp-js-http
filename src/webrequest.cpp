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

#include "webrequest.hpp"
#include "spdlog/spdlog.h"
#include <sstream>

size_t WriteCallback(char *ptr, size_t size, size_t mem, void *param)
{
	size_t totalbytes = size * mem;
	ptr[totalbytes] = '\0';	// null terminate the returned data

	std::string *str = static_cast<std::string*>(param);
	if (str)
		str->assign(ptr);

	return totalbytes;
}

WebRequest::WebRequest(void)
{
    m_Curl = curl_easy_init();
    if (m_Curl)
    {
        curl_easy_setopt(m_Curl, CURLOPT_ERRORBUFFER, m_errorBuf);
		curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(m_Curl, CURLOPT_WRITEDATA, &m_Result);

		std::ostringstream agent;
		agent << "smtp-js-http;";
		agent << "libcurl/" << curl_version_info(CURLVERSION_NOW)->version;
		curl_easy_setopt(m_Curl, CURLOPT_USERAGENT, agent.str().c_str());
    }

    m_Headers = nullptr;
}

WebRequest::~WebRequest(void)
{
	if (m_Headers)
		curl_slist_free_all(m_Headers);
        
    if (m_Curl)
        curl_easy_cleanup(m_Curl);
}

void WebRequest::Header(const std::string &name, const std::string &value)
{
	std::ostringstream oss;
	oss << name << ": " << value;

	m_Headers = curl_slist_append(m_Headers, oss.str().c_str());
}

void WebRequest::PostData(const std::string &data)
{
    m_PostData.assign(data);
}

int32_t WebRequest::Get(const std::string &url)
{
    if (m_Curl)
    {
        curl_easy_setopt(m_Curl, CURLOPT_POST, 0);
        return Perform(url);
    }

    return -1;
}

int32_t WebRequest::Post(const std::string &url)
{
    if (m_Curl)
    {
        curl_easy_setopt(m_Curl, CURLOPT_POST, 1L);
        curl_easy_setopt(m_Curl, CURLOPT_POSTFIELDS, m_PostData.c_str());
        curl_easy_setopt(m_Curl, CURLOPT_POSTFIELDSIZE, -1L);

        return Perform(url);
    }

    return -1;
}

int32_t WebRequest::Perform(const std::string &url)
{
    m_Result.clear();

    if (m_Headers)
        curl_easy_setopt(m_Curl, CURLOPT_HTTPHEADER, m_Headers);

    curl_easy_setopt(m_Curl, CURLOPT_URL, url.c_str());
    CURLcode result = curl_easy_perform(m_Curl);
    if (result == CURLE_OK)
    {
        m_Error.clear();
        return 0;
    }
    else
    {
        m_Error.assign(m_errorBuf);
    }
        
    return result;
}