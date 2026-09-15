cmake_minimum_required(VERSION 3.10)
project(WebServer)

# 使用 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 源文件
add_executable(server
    src/main.cpp
)
