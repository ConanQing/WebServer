#ifndef STATIC_HANDLER_H
#define STATIC_HANDLER_H

#include "HttpRequest.h"
#include "HttpResponse.h"

class StaticHandler
{
public:
    void handle(const HttpRequest& request, HttpResponse& response);
};

#endif
