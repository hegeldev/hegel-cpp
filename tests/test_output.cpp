#include <gtest/gtest.h>

#include <string>

#include "common/subprocess.h"
#include "common/utils.h"

using hegel::tests::common::assert_matches_regex;
using hegel::tests::common::run_subject;
using hegel::tests::common::SubprocessResult;

#ifndef HEGEL_SUBJECT_BIN
#error "HEGEL_SUBJECT_BIN must be defined at build time"
#endif

namespace {
    // Run the prebuilt subject binary for one scenario (see subject_main.cpp).
    // No per-test recompile, so these tests run fast and in parallel.
    SubprocessResult run_scenario(const std::string& name) {
        return run_subject(HEGEL_SUBJECT_BIN, {name});
    }
} // namespace

TEST(Output, FailingTest) {
    SubprocessResult r = run_scenario("failing");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Generated: 0\b)");
    assert_matches_regex(r.stderr_data, R"(intentional failure: 0\b)");
}

TEST(Output, OriginStableAcrossDrawnValues) {
    SubprocessResult r = run_scenario("stable_origin");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Generated: 10\b)");
    assert_matches_regex(r.stderr_data, R"(failure with x=10\b)");
}

// Non-std exceptions carry no message, so only the replay's draw output is
// portable to assert on; the exception itself terminates the subject.
TEST(Output, NonStdExceptionIsHandled) {
    SubprocessResult r = run_scenario("throw_int");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Generated: 5\b)");
}

TEST(Output, CustomNonStdExceptionIsHandled) {
    SubprocessResult r = run_scenario("throw_custom");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Generated: 5\b)");
}

TEST(Output, MultipleFailuresReported) {
    SubprocessResult r = run_scenario("multiple_failures");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data,
                         R"(Hegel test failed with 2 distinct failures)");
    assert_matches_regex(r.stderr_data, R"(Failure 1:)");
    assert_matches_regex(r.stderr_data, R"(Failure 2:)");
    assert_matches_regex(r.stderr_data, R"(even bug with x=10\b)");
    assert_matches_regex(r.stderr_data, R"(odd bug with x=11\b)");
}

TEST(Output, MultipleFailuresOffReportsOne) {
    SubprocessResult r = run_scenario("multiple_failures_off");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(\w+ bug with x=1[01]\b)");
    EXPECT_EQ(r.stderr_data.find("distinct failures"), std::string::npos)
        << r.stderr_data;
}

TEST(Output, ExceptionMessageIsShown) {
    SubprocessResult r = run_scenario("exception_message");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(custom exception for x=7)");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
