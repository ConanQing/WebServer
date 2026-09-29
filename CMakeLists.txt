cmake_minimum_required(VERSION 3.10)

project(ConanWebServer)

set(CMAKE_CXX_STANDARD 17)

# std::thread 需要链接 pthread
find_package(Threads REQUIRED)

add_executable(server
    src/main.cpp
    src/Server.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/Router.cpp
    src/ThreadPool.cpp
    src/handlers/LoginHandler.cpp
    src/handlers/StaticHandler.cpp
)

# 头文件在 src/ 下，告诉编译器也去那里找
target_include_directories(server PRIVATE src)

target_link_libraries(server PRIVATE Threads::Threads)
