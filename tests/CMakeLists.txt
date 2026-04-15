# =============================================================================
# Tests with Google Test
# =============================================================================

enable_testing()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(property_tests property_tests.cpp)
target_link_libraries(property_tests
    PRIVATE
        hegel
        GTest::gtest_main
)

add_executable(test_settings test_settings.cpp)
target_link_libraries(test_settings
    PRIVATE
        hegel
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(property_tests)
gtest_discover_tests(test_settings)

add_executable(test_hegel test_hegel.cpp)
target_link_libraries(test_hegel
    PRIVATE
        hegel
        GTest::gtest_main
)
gtest_discover_tests(test_hegel)


add_executable(test_random test_random.cpp)
target_link_libraries(test_random
    PRIVATE
        hegel
        GTest::gtest_main
)
gtest_discover_tests(test_random)

add_executable(test_no_nlohmann_include test_no_nlohmann_include.cpp)
target_link_libraries(test_no_nlohmann_include
    PRIVATE
        hegel
        GTest::gtest_main
)
gtest_discover_tests(test_no_nlohmann_include)

add_executable(test_derived test_derived.cpp)
target_link_libraries(test_derived
    PRIVATE
        hegel
        GTest::gtest_main
)
gtest_discover_tests(test_derived)

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils
    PRIVATE
        hegel
        GTest::gtest
)
gtest_discover_tests(test_utils)

add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol
    PRIVATE
        hegel
        GTest::gtest
        nlohmann_json::nlohmann_json
)
target_include_directories(test_protocol
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)
gtest_discover_tests(test_protocol)

add_executable(test_coverage test_coverage.cpp)
target_link_libraries(test_coverage
    PRIVATE
        hegel
        GTest::gtest
)
gtest_discover_tests(test_coverage)

add_executable(gtest_failure_test gtest_failure_test.cpp)
target_link_libraries(gtest_failure_test
    PRIVATE
        hegel
        GTest::gtest
)

add_test(NAME gtest_failure_test COMMAND gtest_failure_test)
set_tests_properties(gtest_failure_test PROPERTIES
    PASS_REGULAR_EXPRESSION "Generated:"
)
