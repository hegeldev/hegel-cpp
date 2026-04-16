cmake_minimum_required(VERSION 3.14)
project(hegel_temp_subject CXX)

# This minimal CMake project is configured once at test runtime by
# tests/common/temp_project.h (TempCppProject). The build directory is reused
# across tests in the same gtest binary so per-test cost is just recompiling
# main.cpp + linking against the parent-built libhegel.a.
#
# HEGEL_BUILD_INFO_FILE must be passed via -D and is generated at hegel
# configure time; it sets HEGEL_TEST_INCLUDE_DIRS and HEGEL_TEST_LIBRARIES.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED HEGEL_BUILD_INFO_FILE)
    message(FATAL_ERROR "HEGEL_BUILD_INFO_FILE must be set via -D")
endif()
include("${HEGEL_BUILD_INFO_FILE}")

add_executable(subject main.cpp)
target_include_directories(subject PRIVATE ${HEGEL_TEST_INCLUDE_DIRS})
target_link_libraries(subject PRIVATE ${HEGEL_TEST_LIBRARIES})
