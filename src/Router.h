#ifndef ROUTER_H
#define ROUTER_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ConnectionPool.h"
#include "RedisClient.h"
#include "handlers/LoginHandler.h"
#include "handlers/StaticHandler.h"

class Router
{
public:
    explicit Router(ConnectionPool& pool, RedisClient& redis);

    // 根据 (method, path) 分派，把结果填进 response
    void route(const HttpRequest& request, HttpResponse& response);

private:
    LoginHandler login_;
    StaticHandler staticFile_;
};

#endif
