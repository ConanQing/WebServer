#include<sys/socket.h>//Socket 相关函数
#include<netinet/in.h>//定义互联网地址（IPv4）的数据结构和字节序转换函数，专门用来处理 IP 地址和端口
#include<arpa/inet.h>//提供 IP 地址格式转换的函数
#include<sys/epoll.h>
#include<iostream>
#include<cstring>
#include<unistd.h>//提供文件描述符操作、进程控制等基础系统函数
#include<string>
#include<sstream>
#include<fstream>

int main()
{

    //创建监听socket
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd == -1)
    {
        std::cerr << "socket failed\n";
        return 1;
    }

    //创建服务器地址
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    //绑定监听socket和服务器地址
    if(bind(listen_fd,
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)) == -1)
    {
        std::cerr << "bind failed\n";
        close(listen_fd);
        return 1;
    }

    std::cout << "bind success\n";

    //监听
    if(listen(listen_fd,128) == -1)
    {
        std::cerr << "listen failed\n";
        close(listen_fd);
        return 1;
    }
    std::cout<<"server is listening on port 8080\n";

    //epoll
    int epoll_fd = epoll_create1(0);

    if(epoll_fd == -1)
    {
        perror("epoll_create1");
        close(listen_fd);
        return 1;
    }

    //准备一个epoll_event
    epoll_event event{};
    event.events = EPOLLIN;//关心这个 fd 有没有“可读事件”
    event.data.fd = listen_fd;//这个事件对应的是 listen_fd

    //EPOLL_CTL_ADD:把一个 fd 加入 epoll 的监视名单
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&event) == -1)
    {
        perror("epoll_ctl");
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }
    //准备一个数组
    epoll_event events[1024];

    //接受客户端请求
    while(true)
    {
        int ready = epoll_wait(epoll_fd,events,1024,-1);

        for(int i = 0;i<ready;++i)
        {
            int temp_fd = events[i].data.fd;
            //有新客户端来了
            if(temp_fd == listen_fd)
            {
                sockaddr_in client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);

                int client_fd = accept(listen_fd,
                    (sockaddr*)&client_addr,
                    &client_addr_len);
                if(client_fd = -1)
                {
                    perror("accept");
                    continue;
                }

                std::cout<<"new client: "<<client_fd<<"\n";
                // 把新客户端 fd 注册进 epoll，监听它的可读事件
                epoll_event client_event{};
                client_event.events = EPOLLIN;
                client_event.data.fd = client_fd;
                if(epoll_ctl(epoll_fd, 
                    EPOLL_CTL_ADD, 
                    client_fd, 
                    &client_event) == -1)
                {
                    perror("epoll_ctl");
                    close(client_fd);
                    continue;
                }

            }
            //已连接客户
            else
            {
                char buffer[4096] = {0};

                int n = recv(temp_fd,buffer,sizeof(buffer)-1,0);
                //客户端断开
                if(n <= 0)
                {
                    close(temp_fd);
                    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,temp_fd,nullptr);
                    std::cout<<"client disconnected: "<<temp_fd<<"\n";
                    continue;
                }

                buffer[n] = '\0';
                std::string body = "Hello from ConanWebServer!";
                std::string response;
                response += "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: text/plain\r\n";
                response += "Content-Length: ";
                response += std::to_string(body.size());
                response += "\r\n";
                response += "\r\n";
                response += body;

                send(temp_fd,response.c_str(),response.size(),0);
            }
        }
    }       
    close(epoll_fd);
    close(listen_fd);
    
    return 0;
}