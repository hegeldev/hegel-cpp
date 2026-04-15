// test_utils.cpp - Tests for hegel::testing utilities

#include <gtest/gtest.h>

#include <hegel/testing.h>

using namespace hegel::generators;
using namespace hegel::testing;

// =============================================================================
// assert_all_examples
// =============================================================================

TEST(TestUtils, AssertAllExamplesPass) {
    assert_all_examples<int>(integers<int>({.min_value = 0, .max_value = 100}),
                             [](int x) { return x >= 0 && x <= 100; });
}

TEST(TestUtils, AssertAllExamplesFail) {
    ASSERT_THROW(assert_all_examples<int>(
                     integers<int>({.min_value = 0, .max_value = 100}),
                     [](int x) { return x < 50; }),
                 std::runtime_error);
}

// =============================================================================
// assert_no_examples
// =============================================================================

TEST(TestUtils, AssertNoExamplesPass) {
    assert_no_examples<int>(integers<int>({.min_value = 0, .max_value = 100}),
                            [](int x) { return x < 0; });
}

TEST(TestUtils, AssertNoExamplesFail) {
    ASSERT_THROW(assert_no_examples<int>(
                     integers<int>({.min_value = 0, .max_value = 100}),
                     [](int x) { return x >= 0; }),
                 std::runtime_error);
}

// =============================================================================
// find_any
// =============================================================================

TEST(TestUtils, FindAnySuccess) {
    auto result =
        find_any<int>(integers<int>({.min_value = 0, .max_value = 100}),
                      [](int x) { return x > 50; });
    ASSERT_GT(result, 50);
}

TEST(TestUtils, FindAnyNoMatch) {
    ASSERT_THROW(find_any<int>(integers<int>({.min_value = 0, .max_value = 10}),
                               [](int x) { return x > 1000; }),
                 std::runtime_error);
}

// =============================================================================
// minimal
// =============================================================================

TEST(TestUtils, MinimalFindsSmallest) {
    auto result =
        minimal<int>(integers<int>({.min_value = 0, .max_value = 1000}),
                     [](int x) { return x > 100; });
    ASSERT_EQ(result, 101);
}

TEST(TestUtils, MinimalNoMatch) {
    ASSERT_THROW(minimal<int>(integers<int>({.min_value = 0, .max_value = 10}),
                              [](int x) { return x > 1000; }),
                 std::runtime_error);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
