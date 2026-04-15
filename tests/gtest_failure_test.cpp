// gtest_failure_test.cpp - Tests that hegel properly reports failures with
// counterexamples when using the HEGEL_TEST macro.
//
// This test intentionally fails to verify that:
// 1. GTest integration properly detects failures via throw_on_failure
// 2. The counterexample is shown in the output (Generated: ...)
//
// This test is registered with PASS_REGULAR_EXPRESSION in CMake, so it
// "passes" when the output matches the expected pattern.

#include <hegel/gtest.h>

HEGEL_TEST(FailureReporting, ShowsCounterexample) {
    int x = hegel::draw(
        hegel::generators::integers<int>({.min_value = 0, .max_value = 100}));
    // This assertion will fail when x > 50, which should happen quickly.
    // The failure message should include the actual value as a counterexample.
    ASSERT_LE(x, 50) << "Value should be <= 50";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
