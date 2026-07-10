// Printing and failure reporting: the human-facing output Hegel emits — failure
// replays, blobs, multiple-failure reports (driven through the subject
// subprocess), and per-verbosity diagnostics (captured in-process).

#include <gtest/gtest.h>

#include <string>

#include <hegel/hegel.h>
#include <hegel/internal.h>

#include "common/subprocess.h"
#include "common/utils.h"

using hegel::tests::common::assert_matches_regex;
using hegel::tests::common::run_subject;
using hegel::tests::common::SubprocessResult;

#ifndef HEGEL_SUBJECT_BIN
#error "HEGEL_SUBJECT_BIN must be defined at build time"
#endif

namespace gs = hegel::generators;

// ---------------------------------------------------------------------------
// Failure output, driven through the prebuilt subject binary
// ---------------------------------------------------------------------------

namespace {
    // Run the prebuilt subject binary for one scenario (see subject_main.cpp).
    // No per-test recompile, so these tests run fast and in parallel.
    SubprocessResult run_scenario(const std::string& name) {
        return run_subject(HEGEL_SUBJECT_BIN, {name});
    }
} // namespace

TEST(Output, PrintBlob) {
    SubprocessResult r = run_scenario("print_blob");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Failure blob:)");
}

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

// ---------------------------------------------------------------------------
// Per-verbosity diagnostics, captured in-process
// ---------------------------------------------------------------------------

namespace {
    constexpr const char* kNote = "SENTINEL_NOTE";

    hegel::Settings with_verbosity(hegel::Verbosity v) {
        return hegel::Settings{.test_cases = 5,
                               .verbosity = v,
                               .database = hegel::Database::disabled()};
    }

    std::string run_capturing_stderr(const hegel::Settings& settings) {
        testing::internal::CaptureStderr();
        hegel::test(
            [](hegel::TestCase& tc) {
                tc.note(kNote);
                (void)tc.draw(gs::integers<int>());
            },
            settings);
        return testing::internal::GetCapturedStderr();
    }

    bool contains(const std::string& haystack, const char* needle) {
        return haystack.find(needle) != std::string::npos;
    }
} // namespace

// Quiet suppresses every per-case diagnostic.
TEST(Diagnostics, QuietSuppressesEverything) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Quiet));
    EXPECT_FALSE(contains(out, kNote));
    EXPECT_FALSE(contains(out, "Generated:"));
}

// Normal does not print per-case notes while the property is passing (notes are
// reserved for the final replay of a counterexample, which a passing run has).
TEST(Diagnostics, NormalSuppressesNotesWhilePassing) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Normal));
    EXPECT_FALSE(contains(out, kNote));
}

// Verbose prints notes and drawn values on every case.
TEST(Diagnostics, VerbosePrintsNotesAndValues) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Verbose));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "Generated:"));
}

// Debug prints everything Verbose does (engine-side shrinker tracing is the
// engine's own output and not asserted on here).
TEST(Diagnostics, DebugPrintsNotesAndValues) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Debug));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "Generated:"));
}

// A throw that isn't a std::exception exercises the catch(...) fallback, which
// records the exception's type name as the failure origin; the single-failure
// re-raise then surfaces the original exception, not a wrapper.
TEST(Diagnostics, NonStandardExceptionOrigin) {
    EXPECT_THROW(hegel::test([](hegel::TestCase&) { throw 42; },
                             with_verbosity(hegel::Verbosity::Quiet)),
                 int);
}

TEST(Diagnostics, InternalExceptionMessages) {
    EXPECT_STREQ(hegel::internal::HegelReject().what(), "test case rejected");
    EXPECT_STREQ(hegel::internal::HegelStopTest().what(),
                 "test case stopped by backend");
}
