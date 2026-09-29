#ifndef LOGIN_HANDLER_H
#define LOGIN_HANDLER_H

#include "HttpRequest.h"
#include "HttpResponse.h"

class LoginHandler
{
public:
    void handle(const HttpRequest& request, HttpResponse& response);
};

#endif
