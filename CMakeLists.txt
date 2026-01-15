cmake_minimum_required(VERSION 3.14)
project(hegel-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(HEGEL_BUILD_EXAMPLES "Build example executables" ON)
option(HEGEL_BUILD_TESTS "Build unit tests" ON)
option(HEGEL_BUILD_CONFORMANCE "Build conformance test binary" ON)
option(HEGEL_COVERAGE "Build with coverage instrumentation" OFF)

# Coverage flags
if(HEGEL_COVERAGE)
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

# =============================================================================
# Hegel Python package installation
# =============================================================================

# Source for hegel Python package (uses SSH for private repo access)
# Can be overridden with -DHEGEL_SOURCE=... to use:
#   - A specific branch: git+ssh://git@github.com/antithesishq/hegel.git@main
#   - A specific tag: git+ssh://git@github.com/antithesishq/hegel.git@v1.0.0
#   - Local path: /path/to/local/hegel
set(HEGEL_SOURCE "git+ssh://git@github.com/antithesishq/hegel.git"
    CACHE STRING "Source for hegel Python package (git URL or local path)")

# First check if hegel is already on PATH
find_program(HEGEL_EXECUTABLE hegel)

if(NOT HEGEL_EXECUTABLE)
  # Find or install uv
  find_program(UV_EXECUTABLE uv)

  if(NOT UV_EXECUTABLE)
    message(STATUS "uv not found, downloading via install script...")

    set(UV_DIR "${CMAKE_BINARY_DIR}/_deps/uv")

    execute_process(
      COMMAND sh -c "curl -LsSf https://astral.sh/uv/install.sh | UV_INSTALL_DIR=${UV_DIR} sh"
      RESULT_VARIABLE UV_INSTALL_RESULT
    )
    if(NOT UV_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to install uv")
    endif()

    set(UV_EXECUTABLE "${UV_DIR}/bin/uv")
  endif()

  message(STATUS "Using uv: ${UV_EXECUTABLE}")

  # Create venv with Python 3.13
  set(HEGEL_VENV_DIR "${CMAKE_BINARY_DIR}/_deps/hegel-venv")

  if(NOT EXISTS "${HEGEL_VENV_DIR}/bin/hegel")
    message(STATUS "Creating hegel venv...")

    execute_process(
      COMMAND ${UV_EXECUTABLE} venv --python 3.13 ${HEGEL_VENV_DIR}
      RESULT_VARIABLE UV_VENV_RESULT
    )
    if(NOT UV_VENV_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to create venv")
    endif()

    # Install hegel from configured source
    message(STATUS "Installing hegel from ${HEGEL_SOURCE}...")

    execute_process(
      COMMAND ${UV_EXECUTABLE} pip install ${HEGEL_SOURCE}
              --python ${HEGEL_VENV_DIR}/bin/python
      RESULT_VARIABLE UV_INSTALL_RESULT
    )
    if(NOT UV_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to install hegel from ${HEGEL_SOURCE}. If using GitHub, ensure you have SSH access to the repository.")
    endif()
  endif()

  set(HEGEL_BINARY_PATH "${HEGEL_VENV_DIR}/bin/hegel")
else()
  set(HEGEL_BINARY_PATH "${HEGEL_EXECUTABLE}")
  message(STATUS "Found hegel on PATH: ${HEGEL_BINARY_PATH}")
endif()

message(STATUS "Hegel binary: ${HEGEL_BINARY_PATH}")

# =============================================================================
# Dependencies via FetchContent
# =============================================================================

include(FetchContent)

# Fetch reflect-cpp
FetchContent_Declare(
    reflectcpp
    GIT_REPOSITORY https://github.com/getml/reflect-cpp.git
    GIT_TAG v0.22.0
)

# reflect-cpp options - we only need JSON support
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BSON OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_CBOR OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_FLEXBUFFERS OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_MSGPACK OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_TOML OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_UBJSON OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_XML OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_YAML OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(reflectcpp nlohmann_json)

# =============================================================================
# Main library target
# =============================================================================

add_library(hegel STATIC src/hegel.cpp)
target_include_directories(hegel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(hegel PUBLIC reflectcpp nlohmann_json::nlohmann_json)
target_compile_features(hegel PUBLIC cxx_std_20)
target_compile_definitions(hegel PUBLIC
    HEGEL_DEFAULT_PATH="${HEGEL_BINARY_PATH}"
)

# =============================================================================
# Examples
# =============================================================================

if(HEGEL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Tests with Google Test
# =============================================================================

if(HEGEL_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(property_tests tests/property_tests.cpp)
    target_link_libraries(property_tests PRIVATE hegel GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(property_tests)

    add_executable(gtest_failure_test tests/gtest_failure_test.cpp)
    target_link_libraries(gtest_failure_test PRIVATE hegel GTest::gtest)

    add_test(NAME gtest_failure_test COMMAND gtest_failure_test)
    set_tests_properties(gtest_failure_test PROPERTIES
        PASS_REGULAR_EXPRESSION "actual:"
    )
endif()

# =============================================================================
# Conformance Tests
# =============================================================================

if(HEGEL_BUILD_CONFORMANCE)
    add_subdirectory(tests/conformance/cpp)
endif()
