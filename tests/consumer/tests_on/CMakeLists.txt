cmake_minimum_required(VERSION 3.14)
project(hegel_consumer_tests_on LANGUAGES CXX)

if(NOT DEFINED HEGEL_ROOT)
    message(FATAL_ERROR "HEGEL_ROOT must be set to the hegel-cpp source directory")
endif()

# Explicitly force hegel's own tests ON to verify consumers who opt in aren't
# broken (regression test for the tests/CMakeLists.txt CMAKE_SOURCE_DIR bug).
set(HEGEL_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(HEGEL_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)

add_subdirectory(${HEGEL_ROOT} ${CMAKE_BINARY_DIR}/hegel)

add_executable(consumer ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp)
target_link_libraries(consumer PRIVATE hegel)
