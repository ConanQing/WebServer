#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>

#include "Router.h"
#include "ThreadPool.h"



class Server
{
public:

    Server(int port);

    void start();


private:

    void setNonBlocking(int fd);

    void handleAccept();

    void handleRead(int fd);

    void handleWrite(int fd);

    // 处理 eventfd 唤醒：把 worker 算好的响应发出去
    void handleWakeup();


private:
    int port_;
    int listen_fd_;
    int epoll_fd_;

    std::unordered_map<int, std::string> buffers_;
    std::unordered_map<int, std::string> write_buffers_;
    std::unordered_map<int, bool> keep_alive_;

    struct DoneItem
    {
        int fd;               // 哪个连接
        std::string response; // 完整响应字节；空 = 请求非法，主线程关连接
        bool keep_alive;      // 发完之后是否保持连接
    };

    Router router_;

    int event_fd_;                    // 唤醒主线程用的 eventfd

    std::queue<DoneItem> done_queue_; // worker 的成果队列
    std::mutex done_mtx_;             // 保护这个队列（全项目唯一加锁点）

    ThreadPool pool_;                 // ★ 最后声明

};

#endif