#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>

class HttpRequest
{
public:
    bool parse(const std::string& request);

    const std::string& method() const;
    const std::string& path() const;
    const std::string& version() const;
    const std::string& getHeader(const std::string& key) const;

    bool keepAlive() const;

private:
    std::string method_;
    std::string path_;
    std::string version_;

    bool keep_alive_;
    
    std::unordered_map<std::string, std::string> headers_;
};

#endif