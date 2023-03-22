cmake_minimum_required(VERSION 3.24)
project(http_server C)

set(CMAKE_C_STANDARD 11)

add_executable(http_server server.c threadpool.c threadpool.h)
