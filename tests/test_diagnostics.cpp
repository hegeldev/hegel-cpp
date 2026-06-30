#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>
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
    EXPECT_FALSE(contains(out, "REQUEST:"));
}

// Normal does not print per-case notes while the property is passing (notes are
// reserved for the final replay of a counterexample, which a passing run has).
TEST(Diagnostics, NormalSuppressesNotesWhilePassing) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Normal));
    EXPECT_FALSE(contains(out, kNote));
}

// Verbose prints notes and drawn values on every case, but does NOT enable the
// protocol REQUEST/RESPONSE dump (that is Debug-only).
TEST(Diagnostics, VerbosePrintsNotesbutNotProtocol) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Verbose));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "Generated:"));
    EXPECT_FALSE(contains(out, "REQUEST:"));
}

// Debug prints everything Verbose does, plus the protocol REQUEST/RESPONSE
// dump.
TEST(Diagnostics, DebugDumpsProtocol) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Debug));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "Generated:"));
    EXPECT_TRUE(contains(out, "REQUEST:"));
    EXPECT_TRUE(contains(out, "RESPONSE:"));
}

// HEGEL_PROTOCOL_DEBUG turns on the protocol dump independently of verbosity:
// at Normal it would otherwise be off, but the env var enables it.
TEST(Diagnostics, ProtocolDebugFromEnv) {
    const char* prev = std::getenv("HEGEL_PROTOCOL_DEBUG");
    std::string saved = prev ? prev : "";
    bool had = prev != nullptr;

    setenv("HEGEL_PROTOCOL_DEBUG", "TRUE", 1); // case-insensitive "true"/"1"
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Normal));

    if (had) {
        setenv("HEGEL_PROTOCOL_DEBUG", saved.c_str(), 1);
    } else {
        unsetenv("HEGEL_PROTOCOL_DEBUG");
    }

    EXPECT_TRUE(contains(out, "REQUEST:"))
        << "HEGEL_PROTOCOL_DEBUG should enable the protocol dump at Normal";
    EXPECT_TRUE(contains(out, "RESPONSE:"))
        << "HEGEL_PROTOCOL_DEBUG should enable the protocol dump at Normal";
}

// A throw that isn't a std::exception exercises the catch(...) fallback, which
// records the exception's type name as the failure origin.
TEST(Diagnostics, NonStandardExceptionOrigin) {
    EXPECT_THROW(hegel::test([](hegel::TestCase&) { throw 42; },
                             with_verbosity(hegel::Verbosity::Quiet)),
                 std::runtime_error);
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
