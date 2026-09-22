#ifndef ROUTER_H
#define ROUTER_H

#include "HttpRequest.h"
#include "HttpResponse.h"

class Router
{
public:
    // 根据 (method, path) 分派，把结果填进 response
    void route(const HttpRequest& request, HttpResponse& response);

private:
    void handleLogin(const HttpRequest& request, HttpResponse& response);

    void handleStatic(const HttpRequest& request, HttpResponse& response);
};

#endif
