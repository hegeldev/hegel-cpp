// test_gtest.cpp - A property inside a GoogleTest test, end to end.
//
// This test intentionally fails to verify that:
// 1. A failed GoogleTest assertion fails the property
// 2. The counterexample is shown in the output
//
// CMake matches the failing output against "actual:", so this "passes" when
// the assertion inside the property fails.

#include <gtest/gtest.h>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

TEST(FailureReporting, ShowsCounterexample) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto x = tc.draw(
                "x", gs::integers<int>({.min_value = 0, .max_value = 100}));
            // This assertion fails when x > 50, which happens quickly. The
            // failure message names the value Hegel shrank to.
            ASSERT_LE(x, 50) << "Value should be <= 50";
        },
        {.test_cases = 100, .database = hegel::Database::disabled()});
}

// A property run outside any GoogleTest test. The integration finds no
// enclosing test, and the property runs as it does in a binary without
// GoogleTest.
void run_outside_a_test() {
    hegel::test([](hegel::TestCase& tc) { (void)tc.draw(gs::integers<int>()); },
                {.test_cases = 5, .database = hegel::Database::disabled()});
}

int main(int argc, char** argv) {
    run_outside_a_test();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
