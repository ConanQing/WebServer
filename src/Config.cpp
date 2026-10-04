#include "Config.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool loadConfig(const std::string& path, Config& out)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;                       // 文件不存在
    }

    json j;
    try
    {
        file >> j;                          // 直接从文件流解析 JSON
    }
    catch (const json::parse_error&)
    {
        return false;                       // 文件不是合法 JSON
    }

    json srv = j.value("server", json::object());
    json db  = j.value("db",     json::object());

    out.server.port = srv.value("port", 8080);

    out.db.host     = db.value("host", "localhost");
    out.db.user     = db.value("user", "");
    out.db.password = db.value("password", "");
    out.db.database = db.value("database", "");
    out.db.port     = db.value("port", 3306);
    out.db.poolSize = db.value("pool_size", 4);

    json rds = j.value("redis", json::object());

    out.redis.host     = rds.value("host", "localhost");
    out.redis.port     = rds.value("port", 6379);
    out.redis.poolSize = rds.value("pool_size", 4);

    return true;
}
