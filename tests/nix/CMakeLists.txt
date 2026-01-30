cmake_minimum_required(VERSION 3.14)
project(hegel-cpp-nix-test VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add hegel-cpp as a subdirectory
add_subdirectory(hegel-cpp)

# Test executable
add_executable(nix_test main.cpp)
target_link_libraries(nix_test PRIVATE hegel)

# Enable testing
enable_testing()
add_test(NAME nix_integration_test COMMAND nix_test)
