#include "Router.h"

#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============ 分派 ============
void Router::route(const HttpRequest& request, HttpResponse& response)
{
    if (request.method() == "POST" && request.path() == "/login")
    {
        handleLogin(request, response);
        return;
    }

    handleStatic(request, response);
}

// ============ POST /login ============
void Router::handleLogin(const HttpRequest& request, HttpResponse& response)
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

// ============ 其他请求：当静态文件处理 ============
void Router::handleStatic(const HttpRequest& request, HttpResponse& response)
{
    std::string path = request.path();

    if (path == "/")
    {
        path = "/index.html";
    }

    std::string file_path = "www" + path;
    std::ifstream file(file_path);

    if (!file.is_open())
    {
        response.setStatus(404, "Not Found");
        response.setBody("<h1>404 Not Found</h1>");
    }
    else
    {
        std::stringstream ss;
        ss << file.rdbuf();
        std::string body = ss.str();
        response.setStatus(200, "OK");
        response.setBody(body);
    }
}
