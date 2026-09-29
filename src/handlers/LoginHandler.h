#ifndef LOGIN_HANDLER_H
#define LOGIN_HANDLER_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ConnectionPool.h"

class LoginHandler{

public:
    explicit LoginHandler(ConnectionPool& pool);

    void handle(const HttpRequest& request, HttpResponse& response);

private:
    // 查库验证账号密码，对了返回 true
    bool checkPassword(const std::string& username, const std::string& password);
    
    ConnectionPool& pool_; 
};

#endif
