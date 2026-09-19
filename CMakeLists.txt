cmake_minimum_required(VERSION 3.10)

project(ConanWebServer)


set(CMAKE_CXX_STANDARD 17)


add_executable(server
    src/main.cpp
    src/Server.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
)