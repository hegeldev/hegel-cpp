cmake_minimum_required(VERSION 3.14)
project(hegel-cpp VERSION 0.2.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HEGEL_BUILD_TESTS "Build unit tests" ON)
option(HEGEL_BUILD_CONFORMANCE "Build conformance test binary" ON)
option(HEGEL_BUILD_DOCS "Build documentation" OFF)
option(HEGEL_COVERAGE "Build with coverage instrumentation" OFF)

if(HEGEL_COVERAGE)
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

# =============================================================================
# Hegel server binary
# =============================================================================
# If HEGEL_PATH is set (e.g. by a nix build), use it as compile-time default.
# Otherwise the SDK auto-installs at runtime via .hegel/venv.

if(HEGEL_PATH)
  message(STATUS "using provided hegel: ${HEGEL_PATH}")
endif()

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

# reflect-cpp options - we use rfl::json for schema derivation and type conversion
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
set(REFLECTCPP_INSTALL ON)

set(NLOHMANN_JSON_VERSION "3.12.0" CACHE STRING "nlohmann/json version tag")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v${NLOHMANN_JSON_VERSION}
)

FetchContent_MakeAvailable(reflectcpp nlohmann_json)

# =============================================================================
# Main library target
# =============================================================================

add_library(hegel
  STATIC
    src/hegel.cpp
    src/generators.cpp
    src/socket.cpp
    src/data.cpp
    src/internal.cpp
    src/protocol.cpp
    src/connection.cpp
)
target_include_directories(hegel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>

    PRIVATE
        src
)
target_link_libraries(hegel
  PUBLIC reflectcpp
)
target_compile_features(hegel PUBLIC cxx_std_20)
if(HEGEL_PATH)
  target_compile_definitions(hegel PUBLIC HEGEL_DEFAULT_PATH="${HEGEL_PATH}")
endif()

# =============================================================================
# Tests with Google Test
# =============================================================================

if(HEGEL_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
    add_subdirectory(tests)
endif()


# =============================================================================
# Conformance Tests
# =============================================================================

if(HEGEL_BUILD_CONFORMANCE)
    add_subdirectory(tests/conformance/cpp)
endif()

# =============================================================================
# Installation
# =============================================================================

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  # Install the library
  install(TARGETS hegel
    EXPORT hegelTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  # Install the nlohmann library headers
  install(
    DIRECTORY ${nlohmann_json_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # Install headers
  install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # Install CMake config files
  install(EXPORT hegelTargets
      FILE hegelTargets.cmake
      NAMESPACE hegel::
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hegel
  )

  # Generate package config file
  configure_package_config_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/hegelConfig.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfig.cmake
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hegel
  )

  # Generate version file
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfigVersion.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY SameMajorVersion
  )

  # Install config files
  install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfigVersion.cmake
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hegel
  )

  # Install docs
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
      TYPE DOC
  )

endif()

# =============================================================================
# Documentation
# =============================================================================

if(HEGEL_BUILD_DOCS)
    find_package(Doxygen REQUIRED)

    FetchContent_Declare(
        doxygen-awesome-css
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
        GIT_TAG v2.3.4
    )

    FetchContent_MakeAvailable(doxygen-awesome-css)

    set(DOXYGEN_AWESOME_CSS_DIR "${doxygen-awesome-css_SOURCE_DIR}")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(DOXYFILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
    set(DOXYFILE_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

    add_custom_target(docs
        ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM
    )
endif()
