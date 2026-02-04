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

add_executable(test_base64 test_base64.cpp)
target_link_libraries(test_base64 
    PRIVATE 
        hegel 
        GTest::gtest_main
)
target_include_directories(test_base64
    PRIVATE
        ../src        
)

include(GoogleTest)
gtest_discover_tests(property_tests)
gtest_discover_tests(test_settings)
gtest_discover_tests(test_base64)

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
