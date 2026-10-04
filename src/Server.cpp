#include "Server.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Router.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <csignal>
#include <atomic>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 退出标志 + 信号处理器
namespace {

    // 单个请求的最大字节数（头部 + body）—— 防止恶意客户端发超长请求耗尽内存
    const std::size_t MAX_REQUEST_SIZE = 64 * 1024;      // 64 KB

    // 连接空闲多久就关掉（防 Slowloris：连上但不发完请求）
    const int CONN_IDLE_TIMEOUT_SEC = 30;

    std::atomic<bool> g_running{true};

    void handleSignal(int)
    {
        g_running = false;      // 只做这一件事
    }

}

Server::Server(int port, ConnectionPool& pool, RedisClient& redis)
    :
    port_(port),
    listen_fd_(-1),
    epoll_fd_(-1),
    router_(pool, redis),
    event_fd_(-1),
    pool_(4)
{

}

void Server::setNonBlocking(int fd)
{
    int flags = fcntl(
        fd,
        F_GETFL,
        0
    );

    fcntl(
        fd,
        F_SETFL,
        flags | O_NONBLOCK
    );
}

void Server::start()
{
    // socket
    listen_fd_ = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if(listen_fd_ == -1)
    {
        perror("socket");
        return;
    }

    // 允许【接管一个只残留 TIME-WAIT 的端口】，让服务器能立刻重启
    // （必须放在 bind() 之前 —— 冲突检查发生在 bind 里）
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};

    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr =
        INADDR_ANY;

    addr.sin_port =
        htons(port_);

    // bind

    if(bind(
        listen_fd_,
        (sockaddr*)&addr,
        sizeof(addr)
    ) == -1)
    {
        perror("bind");
        return;
    }

    // listen

    if(listen(
        listen_fd_,
        128
    ) == -1)
    {
        perror("listen");
        return;
    }

    // 非阻塞

    setNonBlocking(
        listen_fd_
    );

    // epoll

    epoll_fd_ =
        epoll_create1(0);

    epoll_event event{};

    event.events =
        EPOLLIN | EPOLLET;

    event.data.fd =
        listen_fd_;

    epoll_ctl(
        epoll_fd_,
        EPOLL_CTL_ADD,
        listen_fd_,
        &event
    );

    //创建 eventfd：worker 用它来唤醒主线程
    event_fd_ = eventfd(0, EFD_NONBLOCK);

    if (event_fd_ < 0)
    {
        perror("eventfd");
        return;
    }

    epoll_event ev_wake{};
    ev_wake.events  = EPOLLIN | EPOLLET;
    ev_wake.data.fd = event_fd_;

    epoll_ctl(
        epoll_fd_,
        EPOLL_CTL_ADD,
        event_fd_,
        &ev_wake
    );

    std::cout
        << "server start: "
        << port_
        << std::endl;

    // 注册信号处理器
    std::signal(SIGINT,  handleSignal);     // Ctrl-C
    std::signal(SIGTERM, handleSignal);     // kill / systemctl stop

    // 忽略 SIGPIPE：往已断开的 socket 写时不杀进程，只返回错误码
    std::signal(SIGPIPE, SIG_IGN);

    epoll_event events[1024];

    // 上次检查空闲连接的时间
    auto last_check = std::chrono::steady_clock::now();

    while(g_running)
    {

        // 用 1 秒超时（而不是 -1 永久阻塞）—— 这样才能定期检查空闲连接
        int n =
            epoll_wait(
                epoll_fd_,
                events,
                1024,
                1000
            );

        for(int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            if(fd == listen_fd_)
            {
                handleAccept();
            }
            else if(fd == event_fd_)
            {
                handleWakeup();
            }
            else if(events[i].events & EPOLLOUT)
            {
                handleWrite(fd);
            }
            else
            {
                handleRead(fd);
            }
        }

        // 每 1 秒最多检查一次空闲连接
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check).count() >= 1000)
        {
            checkTimeouts();
            last_check = now;
        }
    }

    std::cout << "[shutdown] 正在优雅退出..." << std::endl;
}

void Server::handleAccept()
{

    while(true)
    {

        sockaddr_in client_addr{};

        socklen_t len =
            sizeof(client_addr);

        int client_fd =
            accept(
                listen_fd_,
                (sockaddr*)&client_addr,
                &len
            );

        if(client_fd == -1)
        {

            if(errno == EAGAIN ||
               errno == EWOULDBLOCK)
            {
                break;
            }

            perror("accept");

            break;

        }

        setNonBlocking(
            client_fd
        );

        buffers_[client_fd]="";
        last_active_[client_fd] = std::chrono::steady_clock::now();

        epoll_event event{};

        event.events =
            EPOLLIN | EPOLLET;

        event.data.fd =
            client_fd;

        epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_ADD,
            client_fd,
            &event
        );

        std::cout
            << "new client: "
            << client_fd
            << std::endl;
    }

}

void Server::handleRead(int fd)
{
    last_active_[fd] = std::chrono::steady_clock::now();   // 刷新活动时间

    char buffer[4096];
    while(true)
    {

        int n =
            recv(
                fd,
                buffer,
                sizeof(buffer),
                0
            );

        if(n > 0)
        {
            buffers_[fd].append(
                buffer,
                n
            );

            // 请求超过大小上限 -> 记日志并关闭连接
            // （严格的实现应该回 413 Payload Too Large，这里从简：直接断）
            if(buffers_[fd].size() > MAX_REQUEST_SIZE)
            {
                std::cerr
                    << "[WARN] 请求超过大小上限("
                    << MAX_REQUEST_SIZE
                    << " 字节)，关闭连接 fd="
                    << fd
                    << std::endl;

                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                buffers_.erase(fd);
                return;
            }
        }

        else if(n == 0)
        {
            close(fd);
            buffers_.erase(fd);
            return;
        }
        else
        {
            if(errno == EAGAIN ||
               errno == EWOULDBLOCK)
            {
                break;
            }

            close(fd);

            buffers_.erase(fd);

            return;
        }
    }

    while(true)
    {
        size_t pos = buffers_[fd].find("\r\n\r\n");
        //判断还有没有完整请求
        if(pos == std::string::npos)
        {
            break;
        }
        //算当前这个完整请求的长度
        size_t header_length = pos + 4;
         // 先把头部单独切出来
        std::string head = buffers_[fd].substr(0, header_length);

        //找有没有body
        size_t body_len = 0;
        size_t cl_pos = head.find("Content-Length:");
        if (cl_pos != std::string::npos)
        {
        try
        {
            body_len = std::stoul(head.substr(cl_pos + 15));// ← 出事点
        }
        catch (...)
        {
            close(fd);              // 关连接
            buffers_.erase(fd);     // 清缓冲区
            return;                 // 结束 handleRead
        }

        }

        size_t request_length = header_length + body_len;

        if (buffers_[fd].size() < request_length)
        {
            break;
        }
        //截取这一个完整请求
        std::string full_request = buffers_[fd].substr(0, request_length);
        //把这一段从缓冲区里删掉
        buffers_[fd].erase(0, request_length);

        pool_.submit([this,fd,full_request]()
        {
            DoneItem item;
            item.fd = fd;
            // 解析 + 拼响应（纯计算，不碰 IO）
            HttpRequest request;

            if (!request.parse(full_request))
            {
                // 请求非法 → 空响应，主线程收到后会关掉这条连接
                item.response   = "";
                item.keep_alive = false;
            }
            else
            {
                HttpResponse response;
                router_.route(request, response);
                response.setKeepAlive(request.keepAlive());

                item.response   = response.toString();
                item.keep_alive = request.keepAlive();
            }

            // 把成果放进队列，交回主线程
            {
                std::lock_guard<std::mutex> lock(done_mtx_);
                done_queue_.push(item);
            }

            // 敲一下 eventfd，唤醒主线程
            std::uint64_t one = 1;
            ssize_t ret = write(event_fd_, &one, sizeof(one));
            (void)ret;

        });

    }
}

void Server::handleWakeup()
{
    // ① 排空 eventfd
    std::uint64_t val;
    while (read(event_fd_, &val, sizeof(val)) > 0)
    {
    }

    // ② 整体搬出来（锁只持有这一瞬间）
    std::queue<DoneItem> local;
    {
        std::lock_guard<std::mutex> lock(done_mtx_);
        local.swap(done_queue_);
    }

    // ③ 循环处理全部
    while (!local.empty())
    {
        DoneItem item = local.front();
        local.pop();

        int fd = item.fd;

        if (buffers_.count(fd) == 0)// 连接已断，丢弃
        {
            continue;
        }

        if (item.response.empty())// 非法请求 → 关闭连接
        {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            buffers_.erase(fd);
            keep_alive_.erase(fd);
            write_buffers_.erase(fd);
            continue;
        }

        write_buffers_[fd] += item.response;// ★ += 不是 =
        keep_alive_[fd] = item.keep_alive;  // ★ 别漏

        epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }
}

void Server::handleWrite(int fd)
{
    std::string& buffer = write_buffers_[fd];

    while (!buffer.empty())
    {
        ssize_t n = send(
            fd,
            buffer.data(),
            buffer.size(),
            MSG_NOSIGNAL
        );

        if (n > 0)
        {
            buffer.erase(0, n);
        }
        else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }
        else
        {
            close(fd);
            buffers_.erase(fd);
            write_buffers_.erase(fd);
            return;
        }
    }

    if (buffer.empty())
    {
        if(keep_alive_[fd])
        {
            epoll_event event{};
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = fd;

            epoll_ctl(
                epoll_fd_,
                EPOLL_CTL_MOD,
                fd,
                &event
            );

            write_buffers_.erase(fd);
        }
        else
        {
            epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_DEL,
            fd,
            nullptr
            );

            close(fd);

            buffers_.erase(fd);
            write_buffers_.erase(fd);
            keep_alive_.erase(fd);
        }
    }
}

// ================= 空闲连接超时检查（防 Slowloris）=================
void Server::checkTimeouts()
{
    auto now = std::chrono::steady_clock::now();

    std::vector<int> expired;

    for (const auto& pair : last_active_)
    {
        auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - pair.second).count();

        if (idle >= CONN_IDLE_TIMEOUT_SEC)
        {
            expired.push_back(pair.first);
        }
    }

    for (int fd : expired)
    {
        std::cerr
            << "[WARN] 连接空闲超时("
            << CONN_IDLE_TIMEOUT_SEC
            << " 秒)，关闭 fd="
            << fd
            << std::endl;

        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        buffers_.erase(fd);
        keep_alive_.erase(fd);
        write_buffers_.erase(fd);
        last_active_.erase(fd);
    }
}
