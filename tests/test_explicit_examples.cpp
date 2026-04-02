#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;

// =============================================================================
// Basic explicit examples (examples-only, no server)
// =============================================================================

TEST(ExplicitExamples, SingleDrawSingleExample) {
    bool ran = false;
    hegel::hegel(
        [&] {
            auto x = hegel::draw(integers<int>());
            EXPECT_EQ(x, 42);
            ran = true;
        },
        {.test_cases = 0, .examples = {{42}}});
    EXPECT_TRUE(ran);
}

TEST(ExplicitExamples, MultipleDraws) {
    bool ran = false;
    hegel::hegel(
        [&] {
            auto x = hegel::draw(integers<int>());
            auto s = hegel::draw(text());
            EXPECT_EQ(x, 7);
            EXPECT_EQ(s, "hello");
            ran = true;
        },
        {.test_cases = 0, .examples = {{7, "hello"}}});
    EXPECT_TRUE(ran);
}

TEST(ExplicitExamples, MultipleExamples) {
    int call_count = 0;
    hegel::hegel(
        [&] {
            auto x = hegel::draw(integers<int>());
            if (call_count == 0) {
                EXPECT_EQ(x, 10);
            } else {
                EXPECT_EQ(x, 20);
            }
            call_count++;
        },
        {.test_cases = 0, .examples = {{10}, {20}}});
    EXPECT_EQ(call_count, 2);
}

TEST(ExplicitExamples, BooleanValues) {
    bool ran = false;
    hegel::hegel(
        [&] {
            auto b = hegel::draw(booleans());
            EXPECT_TRUE(b);
            ran = true;
        },
        {.test_cases = 0, .examples = {{true}}});
    EXPECT_TRUE(ran);
}

TEST(ExplicitExamples, FloatingPointValues) {
    bool ran = false;
    hegel::hegel(
        [&] {
            auto f = hegel::draw(floats());
            EXPECT_DOUBLE_EQ(f, 3.14);
            ran = true;
        },
        {.test_cases = 0, .examples = {{3.14}}});
    EXPECT_TRUE(ran);
}

// =============================================================================
// Error conditions
// =============================================================================

TEST(ExplicitExamples, TooManyValuesThrows) {
    EXPECT_THROW(
        hegel::hegel(
            [&] {
                hegel::draw(integers<int>());
                // Only one draw, but two values provided
            },
            {.test_cases = 0, .examples = {{1, 2}}}),
        std::runtime_error);
}

TEST(ExplicitExamples, TooFewValuesUsesGenerator) {
    // When explicit values run out, draw() falls through to the generator.
    // With connection=nullptr, this will throw (no server).
    EXPECT_THROW(
        hegel::hegel(
            [&] {
                hegel::draw(integers<int>());
                hegel::draw(integers<int>()); // No explicit value left
            },
            {.test_cases = 0, .examples = {{42}}}),
        std::exception);
}

TEST(ExplicitExamples, AssumeFailureIsError) {
    EXPECT_THROW(
        hegel::hegel(
            [&] {
                auto x = hegel::draw(integers<int>());
                hegel::assume(x > 100); // 42 is not > 100
            },
            {.test_cases = 0, .examples = {{42}}}),
        std::runtime_error);
}

// =============================================================================
// Examples with test_cases=0 means examples-only
// =============================================================================

TEST(ExplicitExamples, TestCasesZeroSkipsServer) {
    // This should not attempt to connect to a server
    bool ran = false;
    hegel::hegel(
        [&] {
            auto x = hegel::draw(integers<int>());
            EXPECT_EQ(x, 99);
            ran = true;
        },
        {.test_cases = 0, .examples = {{99}}});
    EXPECT_TRUE(ran);
}

TEST(ExplicitExamples, NoExamplesTestCasesZeroIsNoop) {
    // No examples and test_cases=0 should just return
    hegel::hegel([] {}, {.test_cases = 0});
}

TEST(ExplicitExamples, TestFailureInExamplePropagates) {
    EXPECT_THROW(
        hegel::hegel(
            [&] {
                auto x = hegel::draw(integers<int>());
                if (x == 42) {
                    throw std::runtime_error("found the answer");
                }
            },
            {.test_cases = 0, .examples = {{42}}}),
        std::runtime_error);
}
