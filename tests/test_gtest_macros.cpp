// GoogleTest assertion macros inside a Hegel property: what a failed
// assertion tells Hegel, what reaches the enclosing GoogleTest test, and how
// many bugs a run reports for assertions that fail on different lines.

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

namespace {

    struct RunResult {
        std::string report;   // what the run printed
        std::string rethrown; // the exception the run raised
        int failures;         // the GoogleTest failures that reached the test
    };

    // Runs `body` as a property and holds back whatever reaches the enclosing
    // GoogleTest test, which would otherwise fail it.
    RunResult run(const std::function<void(hegel::TestCase&)>& body,
                  hegel::Settings settings = {}) {
        settings.seed = 1;
        settings.derandomize = false;
        settings.database = hegel::Database::disabled();

        testing::TestPartResultArray recorded;
        RunResult out{"", "<no exception>", 0};
        {
            testing::ScopedFakeTestPartResultReporter reporter(
                testing::ScopedFakeTestPartResultReporter::
                    INTERCEPT_ONLY_CURRENT_THREAD,
                &recorded);
            testing::internal::CaptureStderr();
            try {
                hegel::test(body, settings);
            } catch (const std::exception& e) {
                out.rethrown = e.what();
            }
            out.report = testing::internal::GetCapturedStderr();
        }
        out.failures = recorded.size();
        return out;
    }

    bool contains(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

    auto small_int() {
        return gs::integers<int32_t>({.min_value = 0, .max_value = 100});
    }

} // namespace

// A failed EXPECT_* fails the test case it ran in, so Hegel shrinks to the
// smallest value that breaks the property. The exception it raises carries
// the assertion's own message and source position.
TEST(GTestMacros, FailedExpectFailsTheProperty) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            auto x = tc.draw("x", small_int());
            EXPECT_LE(x, 50) << "x was " << x;
        },
        hegel::Settings{.test_cases = 200});
    EXPECT_TRUE(contains(out.report, "Falsified after")) << out.report;
    EXPECT_TRUE(contains(out.report, "auto x = 51;")) << out.report;
    EXPECT_TRUE(contains(out.rethrown, "test_gtest_macros.cpp:"))
        << out.rethrown;
    EXPECT_TRUE(contains(out.rethrown, "x was 51")) << out.rethrown;
}

// ASSERT_* ends the body it fails in, and a property body is a callable, so
// the rest of that body is skipped. The case still fails.
TEST(GTestMacros, FailedAssertEndsTheBodyAndFailsTheCase) {
    bool reached_end = false;
    RunResult out = run(
        [&reached_end](hegel::TestCase& tc) {
            auto x = tc.draw("x", small_int());
            ASSERT_LT(x, 0); // never true: the generator starts at 0
            reached_end = true;
        },
        hegel::Settings{.test_cases = 50});
    EXPECT_FALSE(reached_end);
    EXPECT_TRUE(contains(out.report, "auto x = 0;")) << out.report;
}

// A case that fails several EXPECT_* raises all of them together, since
// EXPECT_* runs on.
TEST(GTestMacros, EveryFailedExpectOfOneCaseIsRaised) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            (void)tc.draw("x", small_int());
            EXPECT_TRUE(false) << "first";
            EXPECT_TRUE(false) << "second";
        },
        hegel::Settings{.test_cases = 5});
    EXPECT_TRUE(contains(out.rethrown, "first")) << out.rethrown;
    EXPECT_TRUE(contains(out.rethrown, "second")) << out.rethrown;
}

// The run and the shrinker fail the same assertion many times. The enclosing
// GoogleTest test fails once, on the exception Hegel re-raises for the
// counterexample it reports.
TEST(GTestMacros, RepeatedAttemptsDoNotFloodTheEnclosingTest) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            auto x = tc.draw("x", small_int());
            ASSERT_LT(x, 60);
        },
        hegel::Settings{.test_cases = 300});
    EXPECT_EQ(out.failures, 0);
    EXPECT_NE(out.rethrown, "<no exception>");
}

// Two ASSERT_* on different lines are two bugs.
//
// Hegel groups counterexamples by origin. Every failed assertion raises one
// type, so hegel::GTestFailure names the positions of the assertions it
// failed, which keeps the two call sites apart. ASSERT_* ends the body it
// fails in, so no case reaches both and there are exactly two.
TEST(GTestMacros, AssertionsOnDifferentLinesAreDistinctBugs) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            auto x = tc.draw("x", small_int());
            ASSERT_EQ(x % 2, 0) << "odd";
            ASSERT_EQ(x % 5, 0) << "not a multiple of five";
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_TRUE(contains(out.report, "Failure 1 of 2")) << out.report;
    EXPECT_TRUE(contains(out.report, "odd")) << out.report;
    EXPECT_TRUE(contains(out.report, "not a multiple of five")) << out.report;
}

// An origin holds where an assertion is, so one assertion that fails on many
// values stays one bug.
TEST(GTestMacros, OneAssertionIsOneBugAcrossValues) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            auto x = tc.draw("x", small_int());
            ASSERT_LT(x, 60) << "too big: " << x;
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_FALSE(contains(out.report, "Failure 1 of")) << out.report;
}

// GoogleTest reports a failure with no source file as one with an empty file
// name. An origin names its position anyway, so two such failures stay two
// bugs.
TEST(GTestMacros, FailureWithNoSourceFileStillHasAPosition) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            (void)tc.draw("x", small_int());
            ADD_FAILURE_AT("", 0) << "from nowhere";
        },
        hegel::Settings{.test_cases = 5});
    EXPECT_TRUE(contains(out.rethrown, "<unknown>: ")) << out.rethrown;
    EXPECT_TRUE(contains(out.rethrown, "from nowhere")) << out.rethrown;
}

// EXPECT_* runs on, so a case can reach both. An origin is the whole set of
// positions a case failed at, which makes failing both a third bug.
//
// Each assertion here fails on its own set of values: the first on even x,
// the second on x >= 50. A case can therefore fail the first alone, the
// second alone, or both, and the run reports the three separately.
TEST(GTestMacros, FailingBothAssertionsIsAThirdBug) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            auto x = tc.draw("x", small_int());
            EXPECT_NE(x % 2, 0) << "even";
            EXPECT_LT(x, 50) << "too big";
        },
        hegel::Settings{.test_cases = 300, .report_multiple_failures = true});
    EXPECT_TRUE(contains(out.report, "Failure 1 of 3")) << out.report;
}

// A body that throws keeps its own exception, which the report names. It
// leaves the body before any later assertion runs.
TEST(GTestMacros, AThrownExceptionWinsOverALaterAssertion) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            (void)tc.draw("x", small_int());
            throw std::runtime_error("thrown first");
            EXPECT_TRUE(false) << "never runs"; // GCOVR_EXCL_LINE
        },
        hegel::Settings{.test_cases = 5});
    EXPECT_EQ(out.rethrown, "thrown first");
    EXPECT_TRUE(contains(out.report, "std::runtime_error")) << out.report;
}

// A property that holds raises nothing. SUCCEED() records a result too, and
// it is not a failure.
TEST(GTestMacros, PassingAssertionsRaiseNothing) {
    RunResult out = run(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(small_int());
            ASSERT_GE(x, 0);
            ASSERT_LE(x, 100);
            SUCCEED() << "explicitly fine";
        },
        hegel::Settings{.test_cases = 50});
    EXPECT_EQ(out.failures, 0);
    EXPECT_EQ(out.rethrown, "<no exception>");
    EXPECT_FALSE(contains(out.report, "Falsified")) << out.report;
}
