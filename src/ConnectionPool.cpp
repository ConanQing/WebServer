#include "ConnectionPool.h"

ConnectionPool::ConnectionPool(const std::string& host, const std::string& user,
                               const std::string& password, const std::string& database,
                               int port, std::size_t size)
    : host_(host), user_(user), password_(password),
      database_(database), port_(port)
{
    for (std::size_t i = 0; i < size; i++)
    {
        MYSQL* conn = createConnection();
        if (conn == nullptr)
        {
            continue;// 建失败就跳过（或直接报错退出）
        }
        free_conns_.push(conn);
    }
}

MYSQL* ConnectionPool::createConnection()
{
    MYSQL* conn = mysql_init(nullptr);
    if (conn == nullptr)
    {
        return nullptr;
    }

    if (mysql_real_connect(conn, host_.c_str(), user_.c_str(), password_.c_str(),
                           database_.c_str(), port_, nullptr, 0) == nullptr)
    {
        mysql_close(conn);
        return nullptr;
    }

    return conn;
}

MYSQL* ConnectionPool::get()
{
    std::unique_lock<std::mutex> lock(mtx_);

    cv_.wait(lock, [this] {
        return stop_ || !free_conns_.empty();
    });

    if (stop_)
    {
        return nullptr;
    }

    MYSQL* conn = free_conns_.front();
    free_conns_.pop();
    return conn;
}

void ConnectionPool::release(MYSQL* conn)
{
    if (conn == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        free_conns_.push(conn);
    }

    cv_.notify_one();
}


ConnectionPool::~ConnectionPool()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
    }
    cv_.notify_all();

    while (!free_conns_.empty())
    {
        MYSQL* conn = free_conns_.front();
        free_conns_.pop();
        mysql_close(conn);
    }
}
