#include "LoginHandler.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

void LoginHandler::handle(const HttpRequest& request, HttpResponse& response)
{
    response.setContentType("application/json");

    try
    {
        json body = json::parse(request.body());

        std::string username = body.value("username", "");
        std::string password = body.value("password", "");

        json res;

        if (username == "conan" && password == "123456")
        {
            response.setStatus(200, "OK");
            res["code"] = 200;
            res["message"] = "login success";
            res["user"]["username"] = username;
        }
        else
        {
            response.setStatus(401, "Unauthorized");
            res["code"] = 401;
            res["message"] = "wrong username or password";
        }

        response.setBody(res.dump());
    }
    catch (const json::parse_error& e)
    {
        // 客户端发来的不是合法 JSON
        response.setStatus(400, "Bad Request");

        json err;
        err["code"] = 400;
        err["message"] = "invalid json";

        response.setBody(err.dump());
    }
}
