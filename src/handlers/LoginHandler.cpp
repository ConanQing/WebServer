#include "LoginHandler.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>

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

        switch (checkPassword(username, password))
        {
            case LoginResult::Success:
                response.setStatus(200, "OK");
                res["code"] = 200;
                res["message"] = "login success";
                res["user"]["username"] = username;
                break;

            case LoginResult::WrongPassword:
                response.setStatus(401, "Unauthorized");
                res["code"] = 401;
                res["message"] = "wrong username or password";
                break;

            case LoginResult::ServerError:
                // 依赖不可用 —— 是服务器的问题，不是用户输错了
                response.setStatus(503, "Service Unavailable");
                res["code"] = 503;
                res["message"] = "service temporarily unavailable";
                break;
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

LoginHandler::LoginHandler(ConnectionPool& pool) : pool_(pool)
{
    
}


LoginHandler::LoginResult
LoginHandler::checkPassword(const std::string& username, const std::string& password)
{
    // ---------- 借连接 ----------
    MYSQL* conn = pool_.get();
    if (conn == nullptr)
    {
        // 拿不到连接 = 依赖不可用，不是"密码错"
        std::cerr << "[ERROR] checkPassword: 连接池无可用连接" << std::endl;
        return LoginResult::ServerError;
    }

    // ---------- 转义（防 SQL 注入）----------
    std::vector<char> esc_user(username.size() * 2 + 1);
    std::vector<char> esc_pass(password.size() * 2 + 1);

    mysql_real_escape_string(conn, esc_user.data(), username.c_str(), username.size());
    mysql_real_escape_string(conn, esc_pass.data(), password.c_str(), password.size());

    std::string sql =
        "SELECT id FROM users WHERE username = \x27" + std::string(esc_user.data()) +
        "\x27 AND password_hash = SHA2(\x27" + std::string(esc_pass.data()) + "\x27, 256)";

    // 默认按"服务器出错"处理；只有明确查到 / 查不到才改
    LoginResult result = LoginResult::ServerError;

    if (mysql_query(conn, sql.c_str()) != 0)
    {
        std::cerr << "[ERROR] checkPassword: mysql_query 失败: "
                  << mysql_error(conn) << std::endl;
    }
    else
    {
        MYSQL_RES* res = mysql_store_result(conn);

        if (res == nullptr)
        {
            std::cerr << "[ERROR] checkPassword: mysql_store_result 失败: "
                      << mysql_error(conn) << std::endl;
        }
        else
        {
            result = (mysql_num_rows(res) > 0) ? LoginResult::Success
                                               : LoginResult::WrongPassword;
            mysql_free_result(res);
        }
    }

    // 唯一出口：无论成败，一定还连接
    pool_.release(conn);

    return result;
}
