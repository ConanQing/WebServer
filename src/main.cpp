#include "Server.h"
#include "ConnectionPool.h"
#include "RedisClient.h"
#include "Config.h"

#include <iostream>

int main()
{
    Config cfg;

    if (!loadConfig("config.json", cfg))
    {
        std::cerr << "读取 config.json 失败。"
                  << "请从 config.example.json 复制一份并填写数据库信息。"
                  << std::endl;
        return 1;
    }

    // conn_pool 必须声明在 server 之前（后声明的先析构）
    ConnectionPool conn_pool(cfg.db.host, cfg.db.user, cfg.db.password,
                             cfg.db.database, cfg.db.port, cfg.db.poolSize);

    // 同样必须声明在 server 之前（后声明先析构）
    RedisClient redis(cfg.redis.host, cfg.redis.port, cfg.redis.poolSize);

    Server server(cfg.server.port, conn_pool, redis);

    server.start();

    return 0;
}
