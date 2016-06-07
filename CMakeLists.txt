cmake_minimum_required(VERSION 3.5)
project(os_ex5)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lpthread")


add_executable(server Server.cpp)
add_executable(client Client.cpp)