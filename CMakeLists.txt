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
#
# Install the pinned version of hegel-core into .hegel/venv via uv.
# The version file (.hegel/venv/hegel-version) caches the installed commit
# so subsequent configures are fast.  Requires `uv` on PATH.

set(HEGEL_VERSION "6e327df2dd42553de12ace94cfbddfbbd9e4bf50")
set(HEGEL_PYTHON_VERSION "3.13")

set(HEGEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.hegel")
set(HEGEL_VENV_DIR "${HEGEL_DIR}/venv")
set(HEGEL_BINARY_PATH "${HEGEL_VENV_DIR}/bin/hegel")
set(HEGEL_VERSION_FILE "${HEGEL_VENV_DIR}/hegel-version")

# Check cached version
set(HEGEL_CACHED_VERSION "")
if(EXISTS "${HEGEL_VERSION_FILE}")
  file(READ "${HEGEL_VERSION_FILE}" HEGEL_CACHED_VERSION)
  string(STRIP "${HEGEL_CACHED_VERSION}" HEGEL_CACHED_VERSION)
endif()

if(HEGEL_CACHED_VERSION STREQUAL HEGEL_VERSION AND EXISTS "${HEGEL_BINARY_PATH}")
  message(STATUS "hegel ${HEGEL_VERSION} already installed")
else()
  find_program(UV_EXECUTABLE uv REQUIRED)
  message(STATUS "using uv: ${UV_EXECUTABLE}")

  message(STATUS "creating venv at ${HEGEL_VENV_DIR}")
  execute_process(
    COMMAND ${UV_EXECUTABLE} venv --python ${HEGEL_PYTHON_VERSION} ${HEGEL_VENV_DIR}
    RESULT_VARIABLE UV_VENV_RESULT
  )
  if(NOT UV_VENV_RESULT EQUAL 0)
    message(FATAL_ERROR "failed to create venv")
  endif()

  message(STATUS "installing hegel (${HEGEL_VERSION})")

  # Prefer local checkout in external/hegel-core if present
  set(HEGEL_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/hegel-core")
  if(EXISTS "${HEGEL_LOCAL_DIR}/pyproject.toml")
    message(STATUS "using local hegel from ${HEGEL_LOCAL_DIR}")
    set(HEGEL_INSTALL_SOURCE "${HEGEL_LOCAL_DIR}")
  else()
    set(HEGEL_INSTALL_SOURCE "hegel @ git+ssh://git@github.com/antithesishq/hegel-core.git@${HEGEL_VERSION}")
  endif()

  execute_process(
    COMMAND ${UV_EXECUTABLE} pip install ${HEGEL_INSTALL_SOURCE}
            --python ${HEGEL_VENV_DIR}/bin/python
    RESULT_VARIABLE HEGEL_INSTALL_RESULT
  )
  if(NOT HEGEL_INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "failed to install hegel")
  endif()
  if(NOT EXISTS "${HEGEL_BINARY_PATH}")
    message(FATAL_ERROR "hegel not found after installation: ${HEGEL_BINARY_PATH}")
  endif()

  file(WRITE "${HEGEL_VERSION_FILE}" "${HEGEL_VERSION}")
endif()

message(STATUS "using hegel: ${HEGEL_BINARY_PATH}")

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
target_compile_definitions(hegel PUBLIC
    HEGEL_DEFAULT_PATH="${HEGEL_BINARY_PATH}"
)

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
