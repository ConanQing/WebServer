#ifndef LOGIN_HANDLER_H
#define LOGIN_HANDLER_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ConnectionPool.h"
#include "RedisClient.h"

class LoginHandler{

public:
    explicit LoginHandler(ConnectionPool& pool, RedisClient& redis);

    void handle(const HttpRequest& request, HttpResponse& response);

private:
    // 查库验证账号密码，对了返回 true
    // 查库验证账号密码的结果（三态：区分"用户的错"和"服务器的错"）
    enum class LoginResult
    {
        Success,          // 密码正确
        WrongPassword,    // 账号或密码错误
        ServerError       // 查库出错（拿不到连接 / SQL 失败）
    };

    LoginResult checkPassword(const std::string& username, const std::string& password);

    ConnectionPool& pool_;
    RedisClient&    redis_;
};

#endif
