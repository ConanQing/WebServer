#include "HttpResponse.h"

void HttpResponse::setStatus(int code, const std::string& status)
{
    status_code_ = code;
    status_ = status;
}

void HttpResponse::setBody(const std::string& body)
{
    body_ = body;
}

void HttpResponse::setKeepAlive(bool keep_alive)
{
    keep_alive_ = keep_alive;
}

std::string HttpResponse::toString() const
{
    std::string response;

    response += "HTTP/1.1 ";
    response += std::to_string(status_code_);
    response += " ";
    response += status_;
    response += "\r\n";

    response += "Content-Type: text/html\r\n";

    response += "Content-Length: ";
    response += std::to_string(body_.size());
    response += "\r\n";
    // 原来：response += "Connection: keep-alive\r\n";
    if (keep_alive_)
    {
        response += "Connection: keep-alive\r\n";
    }
    else
    {
        response += "Connection: close\r\n";
    }
    
    response += "\r\n";

    response += body_;

    return response;
}