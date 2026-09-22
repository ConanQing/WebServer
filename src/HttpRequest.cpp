#include "HttpRequest.h"

#include <sstream>

bool HttpRequest::parse(const std::string& request)
{
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();

    keep_alive_ = true;

    std::istringstream iss(request);

    iss >> method_
        >> path_
        >> version_;

    if (method_.empty() ||
        path_.empty() ||
        version_.empty())
    {
        return false;
    }

    // 找到请求行结束的位置
    size_t line_end = request.find("\r\n");

    if (line_end == std::string::npos)
    {
        return false;
    }

    // 从请求行后面开始解析 Header
    size_t start = line_end + 2;

    while (true)
    {
        size_t end = request.find(
            "\r\n",
            start
        );

        if (end == std::string::npos)
        {
            break;
        }

        // 空行：Header 解析结束
        if (end == start)
        {
            break;
        }

        std::string line =
            request.substr(
                start,
                end - start
            );

        size_t colon = line.find(":");

        if (colon != std::string::npos)
        {
            std::string key =
                line.substr(0, colon);

            std::string value =
                line.substr(colon + 1);

            // 去掉 value 前面的空格
            if (!value.empty() && value[0] == ' ')
            {
                value.erase(0, 1);
            }

            headers_[key] = value;
        }

        start = end + 2;
    }

    auto it_len = headers_.find("Content-Length");

    if (it_len != headers_.end())
    {
        size_t length = std::stoul(it_len->second);
        size_t body_start = request.find("\r\n\r\n");

        if (body_start != std::string::npos)
        {
            body_start += 4;
            if (request.size() >= body_start + length)
            {
                body_ = request.substr(body_start,length);
            }
        }
    }    

    // 根据 Connection Header 判断是否保持连接
    auto it = headers_.find("Connection");

    if (it != headers_.end())
    {
        if (it->second == "close")
        {
            keep_alive_ = false;
        }
    }

    return true;
}

const std::string& HttpRequest::method() const
{
    return method_;
}

const std::string& HttpRequest::path() const
{
    return path_;
}

const std::string& HttpRequest::version() const
{
    return version_;
}

const std::string& HttpRequest::body() const
{
    return body_;
}

bool HttpRequest::keepAlive() const
{
    return keep_alive_;
}

const std::string& HttpRequest::getHeader(
    const std::string& key) const
{
    static const std::string empty;

    auto it = headers_.find(key);

    if (it == headers_.end())
    {
        return empty;
    }

    return it->second;
}