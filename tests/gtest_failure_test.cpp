// gtest_failure_test.cpp - Tests that hegel properly reports failures with
// counterexamples
//
// This test intentionally fails to verify that:
// 1. GTest integration properly reports failures
// 2. The counterexample is shown in the output
//
// This test is registered with WILL_FAIL in CMake, so it "passes" when it
// fails.

#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;

TEST(FailureReporting, ShowsCounterexample) {
    hegel::hegel(
        [] {
            int x =
                integers<int>({.min_value = 0, .max_value = 100}).generate();
            // This assertion will fail when x > 50, which should happen quickly
            // The failure message should include the actual value as a
            // counterexample
            ASSERT_LE(x, 50) << "Value should be <= 50";
        },
        {.test_cases = 100});
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
