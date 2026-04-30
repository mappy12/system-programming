cmake_minimum_required(VERSION 3.10)
project(Lab4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE pthread)

add_executable(client client.cpp
)
target_link_libraries(client PRIVATE pthread)