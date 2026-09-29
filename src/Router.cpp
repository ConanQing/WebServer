#include "Router.h"

Router::Router(ConnectionPool& pool) : login_(pool)
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
