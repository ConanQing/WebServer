#include<sys/socket.h>//Socket 相关函数
#include<netinet/in.h>//定义互联网地址（IPv4）的数据结构和字节序转换函数，专门用来处理 IP 地址和端口
#include<arpa/inet.h>//提供 IP 地址格式转换的函数

#include<iostream>
#include<cstring>
#include<unistd.h>//提供文件描述符操作、进程控制等基础系统函数
#include<string>
#include<sstream>

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

    //接受客户端请求
    while(true)
    {
        //装客户端的地址，还有长度
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(listen_fd,
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_addr_len);

        if(client_fd == -1)
        {
            perror("accept");
            close(listen_fd);
            return 1;
        }

        std::cout<<"new client connected,client_fd = " <<client_fd<<"\n";

        char buffer[4096] = {0};

        int n = recv(client_fd,buffer,sizeof(buffer) - 1,0);
        if(n == -1)
        {
            perror("recv");
            close(client_fd);
            close(listen_fd);
            return 1;
        }
        else if(n == 0)
        {
            std::cout<<"client disconnected\n";
            close(client_fd);
            close(listen_fd);
            return 0;
        }

        std::cout<<"received:\n";
        std::cout<< buffer <<"\n";

        std::istringstream iss(buffer);
        std::string method;
        std::string path;
        std::string version;

        iss >> method >>path >> version;

        std::string body = "Hello Conan!";

        std::string response;

        response += "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: ";
        response += std::to_string(body.size());
        response += "\r\n";
        response += "\r\n";
        response += body;

        send(client_fd,response.c_str(),response.size(),0);
    }


    return 0;
}