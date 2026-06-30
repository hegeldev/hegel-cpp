cmake_minimum_required(VERSION 3.14)
project(hegel_consumer_cxx17 LANGUAGES CXX)

# Consume hegel from a strict C++17 project. The package must have been built
# and installed with HEGEL_REFLECTION=OFF (no reflect-cpp / default_generator);
# see the check-cxx17 recipe.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(hegel REQUIRED)

add_executable(consumer ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(consumer PRIVATE hegel::hegel)

enable_testing()
add_test(NAME cxx17_consumer COMMAND consumer)
