enable_testing()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Snapshot ("approval") testing for full-output assertions. ApprovalTests
# is fetched at configure time, so sandboxed builds without network access
# (e.g. nix) set HEGEL_APPROVAL_TESTS=OFF; the test suites that contain
# approval tests are skipped there.
option(HEGEL_APPROVAL_TESTS
    "Build the test suites that use ApprovalTests snapshots" ON)
if(HEGEL_APPROVAL_TESTS)
    FetchContent_Declare(
        ApprovalTests
        GIT_REPOSITORY https://github.com/approvals/ApprovalTests.cpp.git
        GIT_TAG v.10.13.0
    )
    FetchContent_MakeAvailable(ApprovalTests)
endif()

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
# hegel_add_test(NAME <name> SOURCE <path> [USE_UTILS] [INTERNAL] [APPROVALS])
#
#   Registers a GTest-based test executable and gtest_discover_tests target.
#
#   NAME         - target name
#   SOURCE       - relative path to the .cpp file
#   USE_UTILS    - if set, link against hegel_tests_common (adds tests/ to
#                  the include path). If not set, link against hegel directly.
#   INTERNAL     - if set, also add src/ to the include path (for unit
#                  tests against private headers).
#   APPROVALS    - if set, the test contains approval (snapshot) tests: the
#                  executable uses common/approval_main.cpp as its main and
#                  links ApprovalTests instead of gtest_main.
# -----------------------------------------------------------------------------
function(hegel_add_test)
    cmake_parse_arguments(HAT
        "USE_UTILS;INTERNAL;APPROVALS"
        "NAME;SOURCE"
        ""
        ${ARGN})

    if(HAT_APPROVALS AND NOT HEGEL_APPROVAL_TESTS)
        return()
    endif()

    set(HAT_SOURCES ${HAT_SOURCE})
    if(HAT_APPROVALS)
        list(APPEND HAT_SOURCES common/approval_main.cpp)
        set(HAT_MAIN_LIBS ApprovalTests::ApprovalTests)
    else()
        set(HAT_MAIN_LIBS GTest::gtest_main)
    endif()

    add_executable(${HAT_NAME} ${HAT_SOURCES})
    if(HAT_USE_UTILS)
        target_link_libraries(${HAT_NAME}
            PRIVATE hegel_tests_common ${HAT_MAIN_LIBS})
    else()
        target_link_libraries(${HAT_NAME}
            PRIVATE hegel GTest::gtest ${HAT_MAIN_LIBS})
    endif()

    if(HAT_INTERNAL)
        target_include_directories(${HAT_NAME}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/src
        )
    endif()

    gtest_discover_tests(${HAT_NAME})
endfunction()

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

# Example / integration property tests.
hegel_add_test(NAME property_tests SOURCE property_tests.cpp)

# Run configuration: settings, phases, backend, database, health checks,
# reproduce-failure blobs, and enum/environment helpers.
hegel_add_test(NAME test_settings SOURCE test_settings.cpp)

# Generators, split by family.
hegel_add_test(NAME test_generators_numeric
    SOURCE test_generators_numeric.cpp)
hegel_add_test(NAME test_generators_strings
    SOURCE test_generators_strings.cpp)
hegel_add_test(NAME test_generators_collections
    SOURCE test_generators_collections.cpp)
hegel_add_test(NAME test_generators_combinators
    SOURCE test_generators_combinators.cpp)
hegel_add_test(NAME test_generators_random
    SOURCE test_generators_random.cpp)
hegel_add_test(NAME test_stateful
    SOURCE test_stateful.cpp
    USE_UTILS
    APPROVALS)


# Guiding generation: reject(), target(), repeat().
hegel_add_test(NAME test_guiding SOURCE test_guiding.cpp)

# Value printer for draw output.
hegel_add_test(NAME test_repr SOURCE test_repr.cpp)
# GoogleTest assertion macros inside a property body.
hegel_add_test(NAME test_gtest_macros SOURCE test_gtest_macros.cpp)
# Test-case cloning: clone(), spawn()/join(), and clone output approval.
hegel_add_test(NAME test_clone SOURCE test_clone.cpp USE_UTILS APPROVALS)

# default_generator is only available with reflection (C++20).
if(HEGEL_REFLECTION)
    hegel_add_test(NAME test_derived SOURCE test_derived.cpp)
endif()
add_executable(subject subject_main.cpp)
target_link_libraries(subject PRIVATE hegel)

# Printing / failure reporting: subject-subprocess output tests plus in-process
# per-verbosity diagnostics. Needs the subject binary for the output scenarios.
if(HEGEL_APPROVAL_TESTS)
    add_executable(test_printing test_printing.cpp common/approval_main.cpp)
    target_link_libraries(test_printing
        PRIVATE hegel_tests_common ApprovalTests::ApprovalTests)
    add_dependencies(test_printing subject)
    target_compile_definitions(test_printing
        PRIVATE
            HEGEL_SUBJECT_BIN="$<TARGET_FILE:subject>"
    )
    if(NOT HEGEL_THROW_SITE)
        target_compile_definitions(test_printing
            PRIVATE HEGEL_TESTS_NO_THROW_SITE)
    endif()
    gtest_discover_tests(test_printing)
endif()

add_executable(test_gtest test_gtest.cpp)
target_link_libraries(test_gtest
    PRIVATE
        hegel
        GTest::gtest
)

add_test(NAME test_gtest COMMAND test_gtest)
set_tests_properties(test_gtest PROPERTIES
    PASS_REGULAR_EXPRESSION "actual:"
)
