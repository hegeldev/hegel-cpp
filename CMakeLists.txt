cmake_minimum_required(VERSION 3.14)
project(hegel-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HEGEL_BUILD_EXAMPLES "Build example executables" ON)
option(HEGEL_BUILD_TESTS "Build unit tests" ON)
option(HEGEL_BUILD_CONFORMANCE "Build conformance test binary" ON)
option(HEGEL_BUILD_DOCS "Build documentation" OFF)
option(HEGEL_COVERAGE "Build with coverage instrumentation" OFF)

if(HEGEL_COVERAGE)
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

# In order:
# * Prefer `hegel` on PATH
# * If not found, install hegel with uv
#    * Prefer `uv` on PATH
#    * If not found, install uv from installer
#
# All artifacts are installed to `CMAKE_BINARY_DIR/_deps/hegel`.

set(HEGEL_PYTHON_VERSION "3.13")
set(HEGEL_INSTALL_DIR "${CMAKE_BINARY_DIR}/_deps/hegel")

find_program(HEGEL_ON_PATH hegel)

if(HEGEL_ON_PATH)
  message(STATUS "found hegel on path: ${HEGEL_ON_PATH}")
  set(HEGEL_BINARY_PATH "${HEGEL_ON_PATH}")
else()
  set(HEGEL_VENV_DIR "${HEGEL_INSTALL_DIR}/.venv")
  set(HEGEL_BINARY_PATH "${HEGEL_VENV_DIR}/bin/hegel")

  if(EXISTS "${HEGEL_BINARY_PATH}")
    message(STATUS "found hegel: ${HEGEL_BINARY_PATH}")
  else()
    find_program(UV_ON_PATH uv)

    if(UV_ON_PATH)
      message(STATUS "found uv on PATH: ${UV_ON_PATH}")
      set(UV_EXECUTABLE "${UV_ON_PATH}")
    else()
      set(UV_EXECUTABLE "${HEGEL_INSTALL_DIR}/uv")

      if(EXISTS "${UV_EXECUTABLE}")
        message(STATUS "found uv: ${UV_EXECUTABLE}")
      else()
        message(STATUS "installing uv")

        file(MAKE_DIRECTORY "${HEGEL_INSTALL_DIR}")

        execute_process(
          COMMAND sh -c "curl -LsSf https://astral.sh/uv/install.sh | UV_INSTALL_DIR=${HEGEL_INSTALL_DIR} UV_NO_MODIFY_PATH=1 sh"
          RESULT_VARIABLE UV_INSTALL_RESULT
        )
        if(NOT UV_INSTALL_RESULT EQUAL 0)
          message(FATAL_ERROR "uv install script failed")
        endif()
        if(NOT EXISTS "${UV_EXECUTABLE}")
          message(FATAL_ERROR "uv not found at ${UV_EXECUTABLE} after installation")
        endif()
      endif()
    endif()

    message(STATUS "using uv: ${UV_EXECUTABLE}")

    message(STATUS "creating venv at ${HEGEL_VENV_DIR}")

    execute_process(
      COMMAND ${UV_EXECUTABLE} venv --python ${HEGEL_PYTHON_VERSION} ${HEGEL_VENV_DIR}
      RESULT_VARIABLE UV_VENV_RESULT
    )
    if(NOT UV_VENV_RESULT EQUAL 0)
      message(FATAL_ERROR "failed to create venv")
    endif()

    message(STATUS "installing hegel")

    execute_process(
      COMMAND ${UV_EXECUTABLE} pip install git+ssh://git@github.com/antithesishq/hegel.git
              --python ${HEGEL_VENV_DIR}/bin/python
      RESULT_VARIABLE HEGEL_INSTALL_RESULT
    )
    if(NOT HEGEL_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "failed to install hegel")
    endif()
    if(NOT EXISTS "${HEGEL_BINARY_PATH}")
      message(FATAL_ERROR "hegel not found after installation: ${HEGEL_BINARY_PATH}")
    endif()
  endif()
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
set(REFLECTCPP_INSTALL ON)
 
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(reflectcpp nlohmann_json)

# =============================================================================
# Main library target
# =============================================================================

add_library(hegel 
  STATIC 
    src/hegel.cpp 
    src/strategies.cpp 
    src/base64.cpp
    src/socket.cpp
    src/run_state.cpp
    src/internal.cpp
)
target_include_directories(hegel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>

    PRIVATE
        src
)
target_link_libraries(hegel 
  PUBLIC reflectcpp 
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)
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

endif()

# =============================================================================
# Documentation
# =============================================================================

if(HEGEL_BUILD_DOCS)
    find_package(Doxygen REQUIRED)

    FetchContent_Declare(
        doxygen-awesome-css
        URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.3.4.zip
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)
    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR DOXYGEN_AWESOME_CSS_DIR)

    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(DOXYFILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
    set(DOXYFILE_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM
    )
endif()
