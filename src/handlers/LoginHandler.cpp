#include "LoginHandler.h"

#include <nlohmann/json.hpp>
#include <vector>

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

        bool login_ok = checkPassword(username, password);
        if (login_ok)
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

LoginHandler::LoginHandler(ConnectionPool& pool) : pool_(pool)
{
    
}


bool LoginHandler::checkPassword(const std::string& username, const std::string& password)
{
    // ---------- 借连接 ----------
    MYSQL* conn = pool_.get();
    if (conn == nullptr)
    {
        return false;                  // 借不到（池子关了），当登录失败
    }

    // ---------- 转义（防 SQL 注入的关键）----------
    // 缓冲区：原长度 * 2 + 1（最坏情况每个字符都被转义成 2 个，+1 给结尾 \0）
    std::vector<char> esc_user(username.size() * 2 + 1);
    std::vector<char> esc_pass(password.size() * 2 + 1);

    mysql_real_escape_string(conn, esc_user.data(), username.c_str(), username.size());
    mysql_real_escape_string(conn, esc_pass.data(), password.c_str(), password.size());

    // ---------- 拼 SQL（值已转义，安全）----------
    std::string sql =
        "SELECT id FROM users WHERE username = \x27" + std::string(esc_user.data()) +
        "\x27 AND password_hash = SHA2(\x27" + std::string(esc_pass.data()) + "\x27, 256)";

    bool found = false;

    if (mysql_query(conn, sql.c_str()) == 0)
    {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res != nullptr)
        {
            found = (mysql_num_rows(res) > 0);      // 有行 = 账号密码对
            mysql_free_result(res);
        }
    }

    // ★ 唯一出口：无论成败，一定还连接
    pool_.release(conn);

    return found;
}
