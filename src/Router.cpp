#include "Router.h"

Router::Router(ConnectionPool& pool, RedisClient& redis)
    : login_(pool, redis)
{

}
void Router::route(const HttpRequest& request, HttpResponse& response)
{
    if (request.method() == "POST" && request.path() == "/login")
    {
        login_.handle(request, response);
        return;
    }

    staticFile_.handle(request, response);
}
