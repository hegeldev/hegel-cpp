enable_testing()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Interface target shared by all tests that use common/utils.h.
# Provides include path and links gtest for the FAIL() macro.
add_library(hegel_tests_common INTERFACE)
target_include_directories(hegel_tests_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(hegel_tests_common
    INTERFACE
        hegel
        GTest::gtest
)

# -----------------------------------------------------------------------------
# hegel_add_test(NAME <name> SOURCE <path> [USE_UTILS] [INTERNAL])
#
#   Registers a GTest-based test executable and gtest_discover_tests target.
#
#   NAME         - target name
#   SOURCE       - relative path to the .cpp file
#   USE_UTILS    - if set, link against hegel_tests_common (adds tests/ to
#                  the include path). If not set, link against hegel directly.
#   INTERNAL     - if set, also add src/ and nlohmann_json to the include
#                  path (for unit tests against private headers).
# -----------------------------------------------------------------------------
function(hegel_add_test)
    cmake_parse_arguments(HAT
        "USE_UTILS;INTERNAL"
        "NAME;SOURCE"
        ""
        ${ARGN})

    add_executable(${HAT_NAME} ${HAT_SOURCE})
    if(HAT_USE_UTILS)
        target_link_libraries(${HAT_NAME}
            PRIVATE hegel_tests_common GTest::gtest_main)
    else()
        target_link_libraries(${HAT_NAME}
            PRIVATE hegel GTest::gtest_main)
    endif()

    if(HAT_INTERNAL)
        target_include_directories(${HAT_NAME}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
                $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
        )
    endif()

    gtest_discover_tests(${HAT_NAME})
endfunction()

hegel_add_test(NAME test_crc32 SOURCE unit/test_crc32.cpp INTERNAL)
hegel_add_test(NAME test_protocol SOURCE unit/test_protocol.cpp INTERNAL)
hegel_add_test(NAME test_utils SOURCE unit/test_utils.cpp INTERNAL)

foreach(name IN ITEMS integers floats strings collections flatmap
        mixed_types)
    hegel_add_test(NAME test_shrink_quality_${name}
        SOURCE shrink_quality/test_${name}.cpp
        USE_UTILS)
endforeach()

foreach(name IN ITEMS integers floats strings collections one_of)
    hegel_add_test(NAME test_find_quality_${name}
        SOURCE find_quality/test_${name}.cpp
        USE_UTILS)
endforeach()

hegel_add_test(NAME property_tests SOURCE property_tests.cpp)
hegel_add_test(NAME test_settings SOURCE test_settings.cpp)
hegel_add_test(NAME test_hegel SOURCE test_hegel.cpp)
hegel_add_test(NAME test_random SOURCE test_random.cpp)
hegel_add_test(NAME test_no_nlohmann_include SOURCE test_no_nlohmann_include.cpp)
hegel_add_test(NAME test_derived SOURCE test_derived.cpp)
hegel_add_test(NAME test_coverage SOURCE test_coverage.cpp)

# The `subject` binary used by TempCppProject is defined as a regular target
# in the parent build tree rather than a separate cmake project. This way it
# automatically inherits every toolchain detail the parent uses to build
# libhegel.a (compiler, CMAKE_CXX_FLAGS, macOS arch/sysroot, etc.), and there
# is no ABI mismatch when the parent is configured with e.g.
# -DCMAKE_CXX_FLAGS="-stdlib=libc++". At test time TempCppProject overwrites
# the source file and invokes `cmake --build --target subject`.
set(HEGEL_TEMP_PROJECT_MAIN_CPP
    ${CMAKE_CURRENT_BINARY_DIR}/temp_project_subject/main.cpp)
if(NOT EXISTS ${HEGEL_TEMP_PROJECT_MAIN_CPP})
    file(WRITE ${HEGEL_TEMP_PROJECT_MAIN_CPP}
        "// Overwritten at test runtime by TempCppProject.\n"
        "int main() { return 0; }\n")
endif()
add_executable(subject EXCLUDE_FROM_ALL ${HEGEL_TEMP_PROJECT_MAIN_CPP})
target_link_libraries(subject PRIVATE hegel)

add_executable(test_output test_output.cpp)
target_link_libraries(test_output
    PRIVATE hegel_tests_common GTest::gtest_main)
target_compile_definitions(test_output
    PRIVATE
        HEGEL_TEMP_PROJECT_MAIN_CPP="${HEGEL_TEMP_PROJECT_MAIN_CPP}"
        HEGEL_TEMP_PROJECT_SUBJECT_BIN="$<TARGET_FILE:subject>"
        HEGEL_TEMP_PROJECT_BUILD_DIR="${CMAKE_BINARY_DIR}"
        HEGEL_TEMP_PROJECT_CMAKE_EXE="${CMAKE_COMMAND}"
)
# Each TEST() in test_output.cpp rewrites the shared subject main.cpp and
# rebuilds the `subject` target, so they cannot run concurrently.
# RESOURCE_LOCK serializes them across ctest -j while still allowing
# parallelism with other test binaries.
gtest_discover_tests(test_output PROPERTIES RESOURCE_LOCK temp_project_subject)

add_executable(gtest_failure_test gtest_failure_test.cpp)
target_link_libraries(gtest_failure_test
    PRIVATE
        hegel
        GTest::gtest
)

add_test(NAME gtest_failure_test COMMAND gtest_failure_test)
set_tests_properties(gtest_failure_test PROPERTIES
    PASS_REGULAR_EXPRESSION "actual:"
)
