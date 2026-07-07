#include <gtest/gtest.h>

#include <string>

#include <hegel/hegel.h>
#include <hegel/internal.h>

namespace gs = hegel::generators;

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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
