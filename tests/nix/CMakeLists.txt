cmake_minimum_required(VERSION 3.14)
project(hegel-cpp-nix-test VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Test executable
add_executable(nix_test main.cpp)
target_link_libraries(nix_test PRIVATE hegel)

# Enable testing
enable_testing()
add_test(NAME nix_integration_test COMMAND nix_test)

install(TARGETS nix_test
    EXPORT nix_test_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
