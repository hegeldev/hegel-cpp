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
#   INTERNAL     - if set, also add src/ to the include path (for unit
#                  tests against private headers).
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

hegel_add_test(NAME property_tests SOURCE property_tests.cpp)
hegel_add_test(NAME test_settings SOURCE test_settings.cpp)
hegel_add_test(NAME test_hegel SOURCE test_hegel.cpp)
hegel_add_test(NAME test_random SOURCE test_random.cpp)
hegel_add_test(NAME test_compose SOURCE test_compose.cpp)
hegel_add_test(NAME test_composite_fallback SOURCE test_composite_fallback.cpp)
hegel_add_test(NAME test_diagnostics SOURCE test_diagnostics.cpp)
hegel_add_test(NAME test_formats SOURCE test_formats.cpp)

# default_generator is only available with reflection (C++20).
if(HEGEL_REFLECTION)
    hegel_add_test(NAME test_derived SOURCE test_derived.cpp)
endif()
add_executable(subject subject_main.cpp)
target_link_libraries(subject PRIVATE hegel)

add_executable(test_output test_output.cpp)
target_link_libraries(test_output
    PRIVATE hegel_tests_common GTest::gtest_main)
add_dependencies(test_output subject)
target_compile_definitions(test_output
    PRIVATE
        HEGEL_SUBJECT_BIN="$<TARGET_FILE:subject>"
)
gtest_discover_tests(test_output)

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
