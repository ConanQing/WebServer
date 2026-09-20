#include"HttpRequest.h"

#include<sstream>

//解析
bool HttpRequest::parse(const std::string& request)
{
    std::istringstream iss(request);
    iss  >> method_ >> path_ >> version_;

    if(method_.empty() || path_.empty() || version_.empty())
    {
        return false;
    }

    return true;
}
//获取方法
const std::string& HttpRequest::method() const
{
    return method_;
}

//获取路径
const std::string& HttpRequest::path() const
{
    return path_;
}

//获取版本
const std::string& HttpRequest::version() const
{
    return version_;
}