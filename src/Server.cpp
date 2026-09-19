#include "Server.h"


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

            int fd =
                events[i].data.fd;



            if(fd == listen_fd_)
            {
                handleAccept();
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




    // HTTP完整判断

    if(buffers_[fd].find("\r\n\r\n")
        == std::string::npos)
    {
        return;
    }




    std::istringstream iss(buffers_[fd]);

    std::string method;
    std::string path;
    std::string version;
    iss >> method
        >> path
        >> version;

    if(path == "/")
    {
        path="/index.html";
    }

    std::string body;

    std::string file_path = "www" + path;
    std::ifstream file(file_path);

    std::string response;

    if(!file.is_open())
    {

        body =
        "<h1>404 Not Found</h1>";
        response +=
        "HTTP/1.1 404 Not Found\r\n";

    }

    else
    {
        std::stringstream ss;
        ss << file.rdbuf();
        body =
            ss.str();

        response +=
        "HTTP/1.1 200 OK\r\n";
    }

    response +=
    "Content-Type: text/html\r\n";

    response +=
    "Content-Length: ";

    response +=
    std::to_string(
        body.size()
    );

    response +=
    "\r\n\r\n";

    response += body;

    send(
        fd,
        response.c_str(),
        response.size(),
        0
    );

    close(fd);

    epoll_ctl(
        epoll_fd_,
        EPOLL_CTL_DEL,
        fd,
        nullptr
    );

    buffers_.erase(fd);
}