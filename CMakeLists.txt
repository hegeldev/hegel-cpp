cmake_minimum_required(VERSION 3.14)
project(hegel-cpp VERSION 0.11.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# don't build tests when consumed via add_subdirectory/FetchContent
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_hegel_default_tests ON)
else()
    set(_hegel_default_tests OFF)
endif()

option(HEGEL_BUILD_TESTS "Build unit tests" ${_hegel_default_tests})
option(HEGEL_BUILD_DOCS "Build documentation" OFF)
option(HEGEL_COVERAGE "Build with coverage instrumentation" OFF)

# Sanitizers to build with, as a -fsanitize argument (e.g. "address,undefined"
# or "thread"). Empty disables. address+undefined and thread are separate,
# mutually exclusive builds; see the `check-sanitizers` just recipe.
set(HEGEL_SANITIZE "" CACHE STRING "Sanitizers to build with (-fsanitize value)")

# default_generator (type-directed derivation) uses reflect-cpp, which requires
# C++20. Turn this OFF to drop reflect-cpp and consume hegel from C++17; you
# lose default_generator but everything else still works.
option(HEGEL_REFLECTION "Enable reflect-cpp powered default_generator (requires C++20)" ON)

# The library builds at C++20; with HEGEL_REFLECTION=OFF the public headers are
# also C++17-consumable (verified by a standalone C++17 consumer in CI), and the
# hegel target advertises cxx_std_17 so C++17 consumers are accepted.
set(CMAKE_CXX_STANDARD 20)

if(HEGEL_COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
    # Lets test binaries that die via abort() (uncaught exceptions) flush
    # their profile data manually; see tests/subject_main.cpp.
    add_compile_definitions(HEGEL_COVERAGE_BUILD)
endif()

if(HEGEL_SANITIZE)
    add_compile_options(-fsanitize=${HEGEL_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${HEGEL_SANITIZE})
endif()

# Acquire libhegel (Hegel's native engine, called in-process via its C ABI)
# and expose it as the imported target hegel::libhegel.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/libhegel.cmake)

# spawn()/Worker in the public headers use std::thread / std::future.
find_package(Threads REQUIRED)

include(FetchContent)

if(HEGEL_REFLECTION)
    FetchContent_Declare(
        reflectcpp
        GIT_REPOSITORY https://github.com/getml/reflect-cpp.git
        GIT_TAG v0.22.0
    )

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
endif()

if(HEGEL_REFLECTION)
    FetchContent_MakeAvailable(reflectcpp)
endif()

add_library(hegel
  STATIC
    src/hegel.cpp
    src/generators.cpp
    src/test_case.cpp
    src/engine.cpp
)
target_include_directories(hegel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>

    PRIVATE
        src
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libhegel>
)
target_link_libraries(hegel
  PUBLIC
    # libhegel resolves the hegel_* symbols. Linked only in the build tree;
    # installed consumers get it re-created by hegelConfig.cmake (which points
    # at the dylib shipped beside the static lib).
    $<BUILD_INTERFACE:hegel::libhegel>
    # spawn()/Worker run test bodies on std::thread; PUBLIC so consumers link it.
    Threads::Threads
)

if(HEGEL_REFLECTION)
    target_link_libraries(hegel PUBLIC reflectcpp)
    target_compile_definitions(hegel PUBLIC HEGEL_HAS_REFLECTION=1)
    # reflect-cpp requires C++20.
    target_compile_features(hegel PUBLIC cxx_std_20)
else()
    target_compile_definitions(hegel PUBLIC HEGEL_HAS_REFLECTION=0)
    # Without reflect-cpp the public headers are C++17-compatible.
    target_compile_features(hegel PUBLIC cxx_std_17)
endif()

if(HEGEL_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
    add_subdirectory(tests)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(TARGETS hegel
    EXPORT hegelTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  # Ship libhegel beside the static archive. The static lib's hegel_* symbols
  # are resolved against this shared library when a consumer links the final
  # executable; hegelConfig.cmake re-creates the imported target pointing here.
  install(FILES "${HEGEL_LIBHEGEL_RESOLVED}"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RENAME "${HEGEL_LIBHEGEL_LOCAL_NAME}"
  )

  install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(EXPORT hegelTargets
      FILE hegelTargets.cmake
      NAMESPACE hegel::
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hegel
  )

  configure_package_config_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/hegelConfig.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfig.cmake
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hegel
      PATH_VARS CMAKE_INSTALL_LIBDIR
  )

  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfigVersion.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY SameMajorVersion
  )

  install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/hegelConfigVersion.cmake
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hegel
  )

  if(HEGEL_BUILD_DOCS)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
        TYPE DOC
    )
  endif()

endif()

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
