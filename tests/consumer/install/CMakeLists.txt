cmake_minimum_required(VERSION 3.14)
project(hegel_consumer_install LANGUAGES CXX)

find_package(hegel REQUIRED)

add_executable(consumer ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp)
target_link_libraries(consumer PRIVATE hegel::hegel)
