#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>

class HttpResponse
{
public:
    //设置200 OK or 404 Not Found
    void setStatus(int code, const std::string& status);
    //设置真正返回给浏览器的 HTML
    void setBody(const std::string& body);
    //设置ContentType
    void setContentType(const std::string& type);
    //拼成完整 HTTP 响应
    std::string toString() const;

    void setKeepAlive(bool keep_alive);
private:
    int status_code_ = 200;

    std::string status_ = "OK";

    std::string content_type_ = "text/html";

    std::string body_;

    bool keep_alive_ = true;
};

#endif