#include "LoginHandler.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>
#include "Crypto.h"
#include <cstdlib>

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

LoginHandler::LoginHandler(ConnectionPool& pool, RedisClient& redis)
    : pool_(pool), redis_(redis)
{

}

// 缓存里存的「用户不存在」标记（防缓存穿透）
namespace {
    const std::string NOT_FOUND_MARKER = "__NOT_FOUND__";
}

LoginHandler::LoginResult
LoginHandler::checkPassword(const std::string& username, const std::string& password)
{
    const std::string cache_key = "user:" + username;

    // ---------- ① 先查 Redis 缓存 ----------
    std::string cached;

    if (redis_.get(cache_key, cached))
    {
        if (cached == NOT_FOUND_MARKER)
        {
            return LoginResult::WrongPassword;      // 之前查过：这个用户不存在
        }

        return Crypto::verifyPassword(password, cached) ? LoginResult::Success
                                                       : LoginResult::WrongPassword;
    }

    // ⚠️ get() 返回 false 有两种可能：key 不存在 / Redis 故障 —— 都走查库
    //    所以 Redis 挂了服务【自动降级】为每次都查 MySQL，不会整体不可用

    // ---------- ② 缓存未命中 → 查 MySQL ----------
    MYSQL* conn = pool_.get();
    if (conn == nullptr)
    {
        std::cerr << "[ERROR] checkPassword: 连接池无可用连接" << std::endl;
        return LoginResult::ServerError;
    }

    // 转义（防 SQL 注入）
    std::vector<char> esc_user(username.size() * 2 + 1);
    mysql_real_escape_string(conn, esc_user.data(), username.c_str(), username.size());

    // 只取 password_hash，比对在 C++ 里做（缓存命中时用不了 MySQL 的 SHA2 函数）
    std::string sql = "SELECT password_hash FROM users WHERE username = \x27" +
                      std::string(esc_user.data()) + "\x27";

    bool        query_ok = false;
    bool        found    = false;
    std::string stored;

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
            MYSQL_ROW row = mysql_fetch_row(res);

            if (row != nullptr && row[0] != nullptr)
            {
                stored = row[0];
                found  = true;
            }

            mysql_free_result(res);
            query_ok = true;
        }
    }

    pool_.release(conn);            // 唯一出口，一定还连接

    if (!query_ok)
    {
        return LoginResult::ServerError;
    }

    // ---------- ③ 写回缓存 ----------
    if (found)
    {
        int ttl = 300 + (rand() % 60);      // 300~360 秒，加抖动防【缓存雪崩】
        redis_.set(cache_key, stored, ttl); // 写失败不影响本次结果

        return Crypto::verifyPassword(password, stored) ? LoginResult::Success
                                                       : LoginResult::WrongPassword;
    }
    else
    {
        // 防【缓存穿透】：把「用户不存在」也缓存起来（短 TTL）
        redis_.set(cache_key, NOT_FOUND_MARKER, 60);

        return LoginResult::WrongPassword;
    }
}
