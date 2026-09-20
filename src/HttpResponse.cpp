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

    response += "\r\n";

    response += body_;

    return response;
}