cmake_minimum_required(VERSION 3.10)
project(echo_server)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server src/server.cpp src/connection.cpp src/request.cpp src/utils.cpp src/heap.cpp)