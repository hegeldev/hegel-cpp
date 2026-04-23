cmake_minimum_required(VERSION 3.14)
project(hegel_consumer_subdirectory LANGUAGES CXX)

if(NOT DEFINED HEGEL_ROOT)
    message(FATAL_ERROR "HEGEL_ROOT must be set to the hegel-cpp source directory")
endif()

add_subdirectory(${HEGEL_ROOT} ${CMAKE_BINARY_DIR}/hegel)

add_executable(consumer ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp)
target_link_libraries(consumer PRIVATE hegel)
