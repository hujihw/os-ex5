cmake_minimum_required(VERSION 3.5)
project(os_ex5)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lpthread -g -Wextra")

add_executable(emServer emServer.cpp)
add_executable(emClient emClient.cpp)
#add_executable(ClientTester ClientTester.cpp)
