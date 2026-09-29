#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct DbConfig
{
    std::string host     = "localhost";
    std::string user;
    std::string password;
    std::string database;
    int         port     = 3306;
    int         poolSize = 4;
};

struct ServerConfig
{
    int port = 8080;
};

struct Config
{
    ServerConfig server;
    DbConfig     db;
};

// 从文件读配置；成功返回 true
bool loadConfig(const std::string& path, Config& out);

#endif
