cmake_minimum_required(VERSION 3.10)
project(Lab3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2)
#add_compile_options("$<$<CONFIG:Release>:-O2>")

find_package(Threads REQUIRED)

add_executable(task1 task1.cpp)
target_link_libraries(task1 Threads::Threads)

add_executable(task2 task2.cpp)
target_link_libraries(task2 Threads::Threads)

add_executable(task3 task3.cpp)
target_link_libraries(task3 Threads::Threads)