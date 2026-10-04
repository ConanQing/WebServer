#include "StaticHandler.h"

#include <fstream>
#include <sstream>

void StaticHandler::handle(const HttpRequest& request, HttpResponse& response)
{
    std::string path = request.path();

    if (path == "/")
    {
        path = "/index.html";
    }

    // 防目录穿越：拒绝包含 ".." 的路径
    // 否则 GET /../../etc/passwd 会被拼成 www/../../etc/passwd，读到系统文件
    if (path.find("..") != std::string::npos)
    {
        response.setStatus(403, "Forbidden");
        response.setBody("<h1>403 Forbidden</h1>");
        return;
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
