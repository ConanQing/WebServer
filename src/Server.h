#ifndef SERVER_H
#define SERVER_H


#include <string>
#include <unordered_map>


class Server
{
public:

    Server(int port);

    void start();


private:

    void setNonBlocking(int fd);

    void handleAccept();

    void handleRead(int fd);


private:

    int port_;

    int listen_fd_;

    int epoll_fd_;


    std::unordered_map<int, std::string> buffers_;
};


#endif#ifndef SERVER_H
#define SERVER_H


#include <string>
#include <unordered_map>


class Server
{
public:

    Server(int port);

    void start();


private:

    void setNonBlocking(int fd);

    void handleAccept();

    void handleRead(int fd);


private:

    int port_;

    int listen_fd_;

    int epoll_fd_;


    std::unordered_map<int, std::string> buffers_;
};


#endif