#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include<mysql/mysql.h>

#include<queue>
#include<mutex>
#include<condition_variable>
#include<string>
#include<cstddef>

class ConnectionPool{
public:
    ConnectionPool(const std::string& host,const std::string& user,
    const std::string& password,const std::string& database,int port,
    std::size_t size);

    ~ConnectionPool();

    MYSQL* get();

    void release(MYSQL* conn);

private:
    // 建一条新连接，失败返回 nullptr
    MYSQL* createConnection();

    private:
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    int port_;

    std::queue<MYSQL*> free_conns_;    // 空闲连接（类比 tasks_）
    std::mutex mtx_;                   // 保护它（类比 mtx_）
    std::condition_variable cv_;       // 池空时等（类比 cv_）
    bool stop_ = false;                // 停机标志
};

#endif