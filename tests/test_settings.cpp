#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <hegel/hegel.h>
#include <hegel/internal.h>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace gs = hegel::generators;
using hegel::Database;
using hegel::HealthCheck;
using hegel::Settings;

TEST(Settings, DefaultRuns100TestCases) {
    int count = 0;
    hegel::test([&count](hegel::TestCase& tc) {
        tc.draw(gs::integers<int>());
        count++;
    });
    EXPECT_EQ(count, 100);
}

TEST(Settings, CustomTestCases) {
    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.test_cases = 17});
    EXPECT_EQ(count, 17);
}

TEST(Settings, SeedDeterminism) {
    auto run = []() {
        std::vector<int32_t> seen;
        hegel::test(
            [&seen](hegel::TestCase& tc) {
                seen.push_back(tc.draw(gs::integers<int32_t>()));
            },
            Settings{.test_cases = 25, .seed = 42});
        return seen;
    };
    auto a = run();
    auto b = run();
    EXPECT_EQ(a, b);
}

TEST(Settings, DerandomizeProducesRepeatableRuns) {
    auto run = []() {
        std::vector<int32_t> seen;
        hegel::test(
            [&seen](hegel::TestCase& tc) {
                seen.push_back(tc.draw(gs::integers<int32_t>()));
            },
            Settings{.test_cases = 25,
                     .derandomize = true,
                     .database = Database::disabled()});
        return seen;
    };
    auto a = run();
    auto b = run();
    EXPECT_EQ(a, b);
}

TEST(Settings, SuppressFilterTooMuch) {
    // Without suppression, the hypothesis filter_too_much health check fires
    // here because ~10% of draws are rejected. We suppress it and expect
    // the test to pass cleanly.
    hegel::test(
        [](hegel::TestCase& tc) {
            int n =
                tc.draw(gs::integers<int>({.min_value = 0, .max_value = 100}));
            tc.assume(n < 90);
        },
        Settings{.suppress_health_check = {HealthCheck::FilterTooMuch}});
}

TEST(Settings, SuppressMultiple) {
    hegel::test(
        [](hegel::TestCase& tc) {
            int n =
                tc.draw(gs::integers<int>({.min_value = 0, .max_value = 100}));
            tc.assume(n < 90);
        },
        Settings{.suppress_health_check = {HealthCheck::FilterTooMuch,
                                           HealthCheck::TooSlow}});
}

TEST(Settings, SuppressAllWithAllHealthChecks) {
    hegel::test(
        [](hegel::TestCase& tc) {
            int n =
                tc.draw(gs::integers<int>({.min_value = 0, .max_value = 100}));
            tc.assume(n < 90);
        },
        Settings{.suppress_health_check = hegel::all_health_checks()});
}

TEST(Settings, SuppressDataTooLarge) {
    hegel::test(
        [](hegel::TestCase& tc) {
            bool do_big = tc.draw(gs::booleans());
            if (do_big) {
                for (int i = 0; i < 100; ++i) {
                    (void)tc.draw(gs::integers<int32_t>());
                }
            }
        },
        Settings{.test_cases = 15,
                 .suppress_health_check = {HealthCheck::TestCasesTooLarge,
                                           HealthCheck::TooSlow,
                                           HealthCheck::LargeInitialTestCase}});
}

TEST(Settings, SuppressLargeBaseExample) {
    hegel::test(
        [](hegel::TestCase& tc) {
            for (int i = 0; i < 10; ++i) {
                (void)tc.draw(gs::vectors(gs::integers<int32_t>(),
                                          {.min_size = 50, .max_size = 50}));
            }
        },
        Settings{.test_cases = 15,
                 .suppress_health_check = {HealthCheck::LargeInitialTestCase,
                                           HealthCheck::TestCasesTooLarge,
                                           HealthCheck::TooSlow}});
}

static int global_counter = 1;

TEST(FlakyReporting, FlakyReplay) {
    global_counter = 1;
    try {
        hegel::test(
            [&](hegel::TestCase& tc) {
                auto x = tc.draw(gs::integers<int>());
                if (global_counter == 1) {
                    global_counter--;
                    throw std::runtime_error("first test case fails");
                }
            },
            {.test_cases = 1,
             .suppress_health_check = {HealthCheck::LargeInitialTestCase}});
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(
            std::string(e.what()).find("Your test produced different outcomes"),
            std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

TEST(FlakyReporting, FlakyGeneration) {
    global_counter = 0;
    try {
        hegel::test(
            [&](hegel::TestCase& tc) {
                tc.draw(gs::integers<int>({.min_value = global_counter,
                                           .max_value = global_counter + 1}));
                global_counter++;
            },
            {.test_cases = 10,
             .suppress_health_check = {HealthCheck::LargeInitialTestCase}});
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "Your data generation is non-deterministic"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

std::optional<int> failed_value = std::nullopt;

HEGEL_TEST(check_database)(hegel::TestCase& tc) {
    int64_t n = tc.draw(gs::integers<int64_t>());
    if (!(n < 1'000'000)) {
        failed_value = {n};
        throw std::runtime_error("n >= 1_000_000");
    }
}

TEST(Settings, DatabaseReplaysFailure) {
    namespace fs = std::filesystem;

    fs::path db_path = fs::temp_directory_path() /
                       ("hegel_db_settings_test_" + std::to_string(::getpid()));
    fs::remove_all(db_path);
    fs::create_directories(db_path);

    auto run_once = [&](const std::string& key,
                        const std::vector<hegel::Phase> phases) {
        try {
            check_database(Settings{
                .database = Database::from_path(db_path.string()),
                .database_key = key,
                .phases = phases,
            });
        } catch (const std::runtime_error&) { // NOLINT(bugprone-empty-catch)
            // expected: the property fails and hegel::test() rethrows
        }
    };

    // First run fails and shrinks to the minimal failing value.
    run_once("replay-key", hegel::all_phases());
    int64_t shrunk_value = failed_value.value();
    EXPECT_EQ(shrunk_value, 1'000'000);

    failed_value = std::nullopt;

    // Second run with the same key replays the shrunk failure first.
    run_once("replay-key", {hegel::Phase::Reuse});
    int64_t replayed_value = failed_value.value();
    EXPECT_EQ(shrunk_value, replayed_value);

    fs::remove_all(db_path);
}

static int macro_inline_count = 0;

HEGEL_TEST(macro_inline_settings,
           {.test_cases = 17,
            .database = Database::disabled()})(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    macro_inline_count++;
}

TEST(Settings, HegelTestMacroInlineSettings) {
    macro_inline_count = 0;
    macro_inline_settings();
    EXPECT_EQ(macro_inline_count, 17);

    // A Settings passed at the call site replaces the inline default.
    macro_inline_count = 0;
    macro_inline_settings({.test_cases = 5, .database = Database::disabled()});
    EXPECT_EQ(macro_inline_count, 5);
}

TEST(Settings, EmptyPhasesRunNothing) {
    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.database = Database::disabled(), .phases = {}});
    EXPECT_EQ(count, 0);
}

TEST(Settings, GenerateOnlyPhaseRunsFullBudget) {
    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.database = Database::disabled(),
                 .phases = {hegel::Phase::Generate}});
    EXPECT_EQ(count, 100);
}

TEST(Settings, SingleTestCaseModeRunsOneCase) {
    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.database = Database::disabled(),
                 .mode = hegel::Mode::SingleTestCase});
    EXPECT_EQ(count, 1);
}

TEST(Settings, UrandomBackendRuns) {
    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.test_cases = 10,
                 .database = Database::disabled(),
                 .backend = hegel::Backend::Urandom});
    EXPECT_EQ(count, 10);
}

TEST(Settings, DefaultBackendRuns) {
    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.test_cases = 10,
                 .database = Database::disabled(),
                 .backend = hegel::Backend::Default});
    EXPECT_EQ(count, 10);
}

// With Database::unset() the engine falls back to its own default `.hegel`
// directory. Run inside a temporary working directory so the database
// artifacts don't leak into the build tree.
TEST(Settings, UnsetDatabaseUsesEngineDefault) {
    namespace fs = std::filesystem;

    fs::path work = fs::temp_directory_path() /
                    ("hegel_unset_db_test_" + std::to_string(::getpid()));
    fs::remove_all(work);
    fs::create_directories(work);
    fs::path prev = fs::current_path();
    fs::current_path(work);

    int count = 0;
    hegel::test(
        [&count](hegel::TestCase& tc) {
            tc.draw(gs::integers<int>());
            count++;
        },
        Settings{.test_cases = 5, .database = Database::unset()});

    fs::current_path(prev);
    fs::remove_all(work);
    EXPECT_EQ(count, 5);
}

// ---------------------------------------------------------------------------
// Reproduce-failure blobs
// ---------------------------------------------------------------------------

TEST(FailureBlobs, BlobDoesNotReproduceFailure) {
    try {
        hegel::test(
            [&](hegel::TestCase& tc) {
                int n = tc.draw(gs::integers<int>());
                if (n < 50) {
                    throw std::runtime_error("fail");
                }
            },
            {}, {"AAEAAAAACgEAAAAy"});
        FAIL();
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()),
                  "The failure blob did not reproduce an error");
    }
}

TEST(FailureBlobs, InvalidBlob) {
    try {
        hegel::test(
            [&](hegel::TestCase& tc) {
                int n = tc.draw(gs::integers<int>());
                if (n < 50) {
                    throw std::runtime_error("fail");
                }
            },
            {}, {"A"});
        FAIL();
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()),
                  "invalid argument: hegel_test_case_from_blob: the supplied "
                  "failure blob could not be decoded. It may be corrupt or "
                  "from an incompatible Hegel version.");
    }
}

HEGEL_REPRODUCE_FAILURE(obvious_fail, "AAEAAAAACgEAAAAA", "invalid")
HEGEL_TEST(obvious_fail,
           {.phases = {hegel::Phase::Explicit}})(hegel::TestCase& tc) {
    int n = tc.draw(gs::integers<int>());
    if (n < 50) {
        throw std::runtime_error("fail");
    }
}

TEST(FailureBlobs, BlobReproduceFailure) {
    try {
        obvious_fail();
        FAIL();
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "fail");
    }
}

// ---------------------------------------------------------------------------
// Enum / environment string helpers
// ---------------------------------------------------------------------------

TEST(Settings, VerbosityToString) {
    using hegel::Verbosity;
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Quiet), "quiet");
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Verbose), "verbose");
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Debug), "debug");
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Normal), "normal");
}

TEST(Settings, HealthCheckToString) {
    using hegel::HealthCheck;
    EXPECT_STREQ(hegel::health_check_to_string(HealthCheck::FilterTooMuch),
                 "filter_too_much");
    EXPECT_STREQ(hegel::health_check_to_string(HealthCheck::TooSlow),
                 "too_slow");
    EXPECT_STREQ(hegel::health_check_to_string(HealthCheck::TestCasesTooLarge),
                 "test_cases_too_large");
    EXPECT_STREQ(
        hegel::health_check_to_string(HealthCheck::LargeInitialTestCase),
        "large_initial_test_case");
    // An out-of-range value falls through the switch to the empty string.
    EXPECT_STREQ(hegel::health_check_to_string(static_cast<HealthCheck>(999)),
                 "");
}

// in_ci() scans known CI environment variables. Drive it with a controlled
// environment so the result doesn't depend on where the suite runs.
TEST(Settings, InCiDetection) {
    static const char* kCiVars[] = {"CI",
                                    "TF_BUILD",
                                    "BUILDKITE",
                                    "CIRCLECI",
                                    "CIRRUS_CI",
                                    "CODEBUILD_BUILD_ID",
                                    "GITHUB_ACTIONS",
                                    "GITLAB_CI",
                                    "HEROKU_TEST_RUN_ID",
                                    "TEAMCITY_VERSION"};

    // Save and clear the ambient CI variables so the test is deterministic.
    std::map<std::string, std::string> saved;
    for (const char* name : kCiVars) {
        if (const char* v = std::getenv(name)) {
            saved.emplace(name, v);
        }
        unsetenv(name);
    }

    // Nothing set: not in CI.
    EXPECT_FALSE(hegel::internal::in_ci());

    // A presence-only variable (expected == nullptr) satisfies the check.
    setenv("CI", "anything", 1);
    EXPECT_TRUE(hegel::internal::in_ci());
    unsetenv("CI");

    // A variable with an expected value matches only when it is equal.
    setenv("GITHUB_ACTIONS", "true", 1);
    EXPECT_TRUE(hegel::internal::in_ci());
    setenv("GITHUB_ACTIONS", "false", 1);
    EXPECT_FALSE(hegel::internal::in_ci());
    unsetenv("GITHUB_ACTIONS");

    // Restore the original environment.
    for (const char* name : kCiVars) {
        unsetenv(name);
    }
    for (const auto& [name, value] : saved) {
        setenv(name.c_str(), value.c_str(), 1);
    }
}
