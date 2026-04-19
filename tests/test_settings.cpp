#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <hegel/hegel.h>
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

// Mirrors rust/tests/test_database_key.rs. After a failing test is shrunk, the
// minimal counterexample should be replayed first on the next run that points
// at the same database directory.
//
// XFAIL: Settings does not yet expose a `database_key`. The server treats
// a null database_key as "don't persist", so the replay never happens. The
// replay assertion below is wrapped in EXPECT_NONFATAL_FAILURE so that this
// test passes today and will start failing (i.e. notice us) once database_key
// support lands — at which point the wrapper should be removed and the Rust
// test's isolation check (different key → no replay) should be ported too.
TEST(Settings, DatabaseReplaysFailure) {
    namespace fs = std::filesystem;

    fs::path db_path = fs::temp_directory_path() /
                       ("hegel_db_settings_test_" + std::to_string(::getpid()));
    fs::remove_all(db_path);
    fs::create_directories(db_path);

    Settings settings{
        .database = Database::from_path(db_path.string()),
    };

    std::vector<int64_t> values;
    auto run_once = [&] {
        values.clear();
        try {
            hegel::test(
                [&](hegel::TestCase& tc) {
                    int64_t n = tc.draw(gs::integers<int64_t>());
                    values.push_back(n);
                    if (!(n < 1'000'000)) {
                        throw std::runtime_error("n >= 1_000_000");
                    }
                },
                settings);
        } catch (const std::runtime_error&) { // NOLINT(bugprone-empty-catch)
            // expected: the property fails and hegel::test() rethrows
        }
    };

    // First run fails and shrinks to the minimal failing value.
    run_once();
    ASSERT_FALSE(values.empty());
    int64_t shrunk_value = values.back();
    EXPECT_EQ(shrunk_value, 1'000'000);

    // Second run should replay the shrunk failure first. Without database_key
    // support this doesn't happen, so we expect the assertion to fail.
    run_once();
    ASSERT_FALSE(values.empty());
    EXPECT_NONFATAL_FAILURE(EXPECT_EQ(values.front(), shrunk_value), "");

    fs::remove_all(db_path);
}
