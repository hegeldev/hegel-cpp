#pragma once

/**
 * @file gtest.h
 * @brief Google Test integration for Hegel property-based tests.
 *
 * Provides the HEGEL_TEST macro which combines hegel::hegel() with
 * Google Test so that GTest assertions (ASSERT_EQ, EXPECT_TRUE, etc.)
 * work correctly inside property-based tests. Without this integration,
 * GTest assertions use `return` to exit the test function, which simply
 * ends the hegel lambda without signaling a failure to the framework.
 *
 * @code{.cpp}
 * #include <hegel/gtest.h>
 *
 * HEGEL_TEST(MyTest, IntegersArePositive) {
 *     auto x = tc.draw(hegel::generators::integers<int>(
 *         {.min_value = 0, .max_value = 100}));
 *     ASSERT_GE(x, 0);
 * }
 * @endcode
 */

#include "hegel.h"

#include <gtest/gtest.h>

/**
 * @brief Define a Hegel property-based test integrated with Google Test.
 *
 * This macro creates a GTest test case that runs the body as a Hegel
 * property test. GTest assertions (ASSERT_*, EXPECT_*) work correctly:
 * failures are detected by Hegel and trigger shrinking.
 *
 * Inside the body, `tc` is an in-scope `hegel::TestCase&` which you use
 * to draw values (`tc.draw(...)`), filter inputs (`tc.assume(...)`),
 * and record notes (`tc.note(...)`).
 *
 * @param suite The test suite name
 * @param name The test name
 */
#define HEGEL_TEST(suite, name)                                                \
    static void hegel_test_body_##suite##_##name(hegel::TestCase& tc);         \
    TEST(suite, name) {                                                        \
        try {                                                                  \
            hegel::hegel(                                                      \
                [](hegel::TestCase& tc) {                                      \
                    GTEST_FLAG_SET(throw_on_failure, true);                    \
                    hegel_test_body_##suite##_##name(tc);                      \
                    GTEST_FLAG_SET(throw_on_failure, false);                   \
                },                                                             \
                {.test_cases = 100});                                          \
        } catch (const std::exception& e) {                                    \
            GTEST_FLAG_SET(throw_on_failure, false);                           \
            FAIL() << e.what();                                                \
        }                                                                      \
    }                                                                          \
    static void hegel_test_body_##suite##_##name(hegel::TestCase& tc)
