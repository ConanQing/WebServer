#include "RedisClient.h"

#include <sys/time.h>

#include <iostream>
#include <vector>
#include <chrono>

RedisClient::RedisClient(const std::string& host, int port, std::size_t poolSize)
    : host_(host), port_(port)
{
    for (std::size_t i = 0; i < poolSize; i++)
    {
        redisContext* ctx = createConnection();
        if (ctx == nullptr)
        {
            continue;               // 建失败就跳过
        }
        free_conns_.push(ctx);
    }
}

RedisClient::~RedisClient()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
    }
    cv_.notify_all();

    while (!free_conns_.empty())
    {
        redisContext* ctx = free_conns_.front();
        free_conns_.pop();
        redisFree(ctx);
    }
}

redisContext* RedisClient::createConnection()
{
    struct timeval timeout = { 1, 500000 };     // 1.5 秒

    redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);

    if (ctx == nullptr)
    {
        std::cerr << "[ERROR] Redis connect: 内存不足" << std::endl;
        return nullptr;
    }

    if (ctx->err)
    {
        std::cerr << "[ERROR] Redis connect 失败: " << ctx->errstr << std::endl;
        redisFree(ctx);
        return nullptr;
    }

    // ★ 关键：redisConnectWithTimeout 只管【连接阶段】的超时。
    //   redisCommand 的读写超时必须单独设 —— 不设的话，遇到异常情况
    //   （Redis 挂了 / 网络中断）会【无限阻塞】，把 worker 线程全部卡死。
    redisSetTimeout(ctx, timeout);

    return ctx;
}

redisContext* RedisClient::acquire()
{
    std::unique_lock<std::mutex> lock(mtx_);

    // ★ 带超时地等待：池空说明连接可能被销毁且重建失败了
    //   （Redis 挂了）。无限等下去会把 worker 全部卡死 → 服务假死。
    //   超时后返回 nullptr，让上层【降级】去查 MySQL。
    if (!cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return stop_ || !free_conns_.empty();
        }))
    {
        std::cerr << "[WARN] Redis 连接池为空，降级（跳过缓存）" << std::endl;
        return nullptr;
    }

    if (stop_)
    {
        return nullptr;
    }

    redisContext* ctx = free_conns_.front();
    free_conns_.pop();

    lock.unlock();              // ping / 重连是网络操作，先放锁

    // 体检：连接还活着吗？（和 mysql_ping 一个道理）
    redisReply* reply = (redisReply*)redisCommand(ctx, "PING");

    bool alive = (reply != nullptr && reply->type == REDIS_REPLY_STATUS);

    if (reply != nullptr)
    {
        freeReplyObject(reply);
    }

    if (!alive)
    {
        redisFree(ctx);
        ctx = createConnection();
    }

    return ctx;
}

void RedisClient::release(redisContext* ctx)
{
    if (ctx == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        free_conns_.push(ctx);
    }

    cv_.notify_one();
}

bool RedisClient::get(const std::string& key, std::string& value)
{
    redisContext* ctx = acquire();
    if (ctx == nullptr)
    {
        return false;
    }

    bool hit = false;

    // 用 redisCommandArgv：参数和命令分开传，防命令注入
    const char* argv[2]     = { "GET", key.c_str() };
    std::size_t argvlen[2]  = { 3,     key.size() };

    redisReply* reply = (redisReply*)redisCommandArgv(ctx, 2, argv, argvlen);

    if (reply != nullptr)
    {
        if (reply->type == REDIS_REPLY_STRING)
        {
            value.assign(reply->str, reply->len);
            hit = true;
        }
        freeReplyObject(reply);
    }

    release(ctx);
    return hit;
}

bool RedisClient::set(const std::string& key, const std::string& value, int ttl)
{
    redisContext* ctx = acquire();
    if (ctx == nullptr)
    {
        return false;
    }

    bool ok = false;

    if (ttl > 0)
    {
        std::string ttl_str = std::to_string(ttl);

        const char* argv[5]     = { "SET", key.c_str(), value.c_str(), "EX", ttl_str.c_str() };
        std::size_t argvlen[5]  = { 3,     key.size(),  value.size(),  2,    ttl_str.size() };

        redisReply* reply = (redisReply*)redisCommandArgv(ctx, 5, argv, argvlen);
        if (reply != nullptr)
        {
            ok = (reply->type == REDIS_REPLY_STATUS);
            freeReplyObject(reply);
        }
    }
    else
    {
        const char* argv[3]     = { "SET", key.c_str(), value.c_str() };
        std::size_t argvlen[3]  = { 3,     key.size(),  value.size() };

        redisReply* reply = (redisReply*)redisCommandArgv(ctx, 3, argv, argvlen);
        if (reply != nullptr)
        {
            ok = (reply->type == REDIS_REPLY_STATUS);
            freeReplyObject(reply);
        }
    }

    release(ctx);
    return ok;
}

bool RedisClient::del(const std::string& key)
{
    redisContext* ctx = acquire();
    if (ctx == nullptr)
    {
        return false;
    }

    bool ok = false;

    const char* argv[2]     = { "DEL", key.c_str() };
    std::size_t argvlen[2]  = { 3,     key.size() };

    redisReply* reply = (redisReply*)redisCommandArgv(ctx, 2, argv, argvlen);

    if (reply != nullptr)
    {
        ok = (reply->type == REDIS_REPLY_INTEGER);
        freeReplyObject(reply);
    }

    release(ctx);
    return ok;
}
