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
