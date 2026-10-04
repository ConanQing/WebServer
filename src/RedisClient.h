#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <hiredis/hiredis.h>

#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <cstddef>

// Redis 客户端（内部维护一个小连接池，线程安全）
// 和 ConnectionPool 是同一个模式：池 + mutex + condition_variable
class RedisClient
{
public:
    RedisClient(const std::string& host, int port, std::size_t poolSize);

    ~RedisClient();

    // 读：命中返回 true，值写进 value
    bool get(const std::string& key, std::string& value);

    // 写：ttl > 0 时设置过期秒数；ttl <= 0 表示不过期
    bool set(const std::string& key, const std::string& value, int ttl);

    // 删
    bool del(const std::string& key);

private:
    // 借一个连接（池空则等待；失效则重建）
    redisContext* acquire();

    // 还回去
    void release(redisContext* ctx);

    // 建立新连接，失败返回 nullptr
    redisContext* createConnection();

private:
    std::string host_;
    int         port_;

    std::queue<redisContext*> free_conns_;   // 空闲连接
    std::mutex                mtx_;
    std::condition_variable   cv_;
    bool                      stop_ = false;
};

#endif
