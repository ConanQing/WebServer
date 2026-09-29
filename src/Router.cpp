#include "Router.h"

void Router::route(const HttpRequest& request, HttpResponse& response)
{
    if (request.method() == "POST" && request.path() == "/login")
    {
        login_.handle(request, response);
        return;
    }

    staticFile_.handle(request, response);
}
