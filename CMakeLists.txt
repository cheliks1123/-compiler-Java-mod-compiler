cmake_minimum_required(VERSION 3.16)
project(LibGetGui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(libget_gui main.cpp)
target_link_libraries(libget_gui PRIVATE Qt6::Widgets Qt6::Network)
