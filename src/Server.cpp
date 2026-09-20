#include "Server.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>



Server::Server(int port)
    :
    port_(port),
    listen_fd_(-1),
    epoll_fd_(-1)
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



    std::cout
        << "server start: "
        << port_
        << std::endl;



    epoll_event events[1024];



    while(true)
    {

        int n =
            epoll_wait(
                epoll_fd_,
                events,
                1024,
                -1
            );



        for(int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            if(fd == listen_fd_)
            {
                handleAccept();
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
    }
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
        size_t request_length = pos + 4;
        //截取这一个完整请求
        std::string full_request = buffers_[fd].substr(0, request_length);
        //把这一段从缓冲区里删掉
        buffers_[fd].erase(0, request_length);

        //解析请求
        HttpRequest request;
        if (!request.parse(full_request))
        {
            close(fd);
            buffers_.erase(fd);
            return;
        }
        std::cout << "method: "
          << request.method()
          << std::endl;

        std::string path = request.path();
        if (path == "/")
        {
            path = "/index.html";
        }

        std::string file_path = "www" + path;
        std::ifstream file(file_path);
        HttpResponse response;

        if (!file.is_open())
        {
            response.setStatus(404, "Not Found");
            response.setBody("<h1>404 Not Found</h1>");
        }
        else
        {
            std::stringstream ss;
            ss << file.rdbuf();
            std::string body = ss.str();
            response.setStatus(200, "OK");
            response.setBody(body);
        }
        //把客户端的意愿传给响应
        response.setKeepAlive(request.keepAlive());
        //进写缓冲，不急着发
        std::string response_data = response.toString();
        write_buffers_[fd] += response_data;
        keep_alive_[fd] = request.keepAlive();
    }
    if (!write_buffers_[fd].empty())
    {
        epoll_event ev{};
        ev.events   = EPOLLOUT | EPOLLET;
        ev.data.fd  = fd;
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
            0
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