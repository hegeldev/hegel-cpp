// Guiding generation: the TestCase controls that steer the engine rather than
// produce values — reject(), target(), and repeat().

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

// ---------------------------------------------------------------------------
// reject()
// ---------------------------------------------------------------------------

// reject() discards the current case as invalid, the same as assume(false).
// A run where some cases reject still completes without reporting a failure.
TEST(Reject, DiscardsCaseNotFailure) {
    EXPECT_NO_THROW(hegel::test(
        [](hegel::TestCase& tc) {
            int n = tc.draw(gs::integers<int>());
            if (n % 2 != 0) {
                tc.reject();
            }
            // Only even values reach here; the property trivially holds.
        },
        hegel::Settings{.test_cases = 100,
                        .derandomize = true,
                        .database = hegel::Database::disabled()}));
}

// reject() does not return: nothing after it runs. If it returned, the throw
// below would surface as a failure and hegel::test() would re-raise it.
TEST(Reject, DoesNotReturn) {
    EXPECT_NO_THROW(hegel::test(
        [](hegel::TestCase& tc) {
            (void)tc.draw(gs::integers<int>());
            tc.reject();
            throw std::runtime_error("reject() returned"); // unreachable
        },
        hegel::Settings{
            .test_cases = 20,
            .derandomize = true,
            .database = hegel::Database::disabled(),
            .suppress_health_check = {hegel::HealthCheck::FilterTooMuch}}));
}

// ---------------------------------------------------------------------------
// target()
// ---------------------------------------------------------------------------

// A run that records a targeting observation each case completes normally.
// Phase::Target is enabled by default, so the observation is honored.
TEST(Targeting, RecordsObservation) {
    EXPECT_NO_THROW(hegel::test(
        [](hegel::TestCase& tc) {
            int n = tc.draw(gs::integers<int>({.min_value = 0}));
            tc.target(static_cast<double>(n));
        },
        hegel::Settings{.test_cases = 100,
                        .derandomize = true,
                        .database = hegel::Database::disabled()}));
}

// The labelled form is accepted the same way. Distinct labels can be recorded
// on the same case.
TEST(Targeting, LabelledObservations) {
    EXPECT_NO_THROW(hegel::test(
        [](hegel::TestCase& tc) {
            int n = tc.draw(gs::integers<int>({.min_value = 0}));
            tc.target(static_cast<double>(n), "size");
            tc.target(static_cast<double>(-n), "negated");
        },
        hegel::Settings{.test_cases = 100,
                        .derandomize = true,
                        .database = hegel::Database::disabled()}));
}

// A non-finite score is a usage error: the engine rejects it and the C++ layer
// surfaces it as std::invalid_argument.
TEST(Targeting, NonFiniteScoreThrows) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         (void)tc.draw(gs::integers<int>());
                         tc.target(std::numeric_limits<double>::infinity());
                     },
                     hegel::Settings{.test_cases = 1,
                                     .derandomize = true,
                                     .database = hegel::Database::disabled()}),
                 std::invalid_argument);
}

// Each label may be recorded at most once per case; a duplicate is a usage
// error surfaced as std::invalid_argument.
TEST(Targeting, DuplicateLabelThrows) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         (void)tc.draw(gs::integers<int>());
                         tc.target(1.0, "dup");
                         tc.target(2.0, "dup");
                     },
                     hegel::Settings{.test_cases = 1,
                                     .derandomize = true,
                                     .database = hegel::Database::disabled()}),
                 std::invalid_argument);
}

// ---------------------------------------------------------------------------
// repeat()
// ---------------------------------------------------------------------------

// repeat() runs its body in an engine-managed loop that can iterate more than
// once, and control returns to the caller after the loop completes.
TEST(Loop, RunsBodyAndReturns) {
    int max_iters = 0;
    bool returned = false;
    hegel::test(
        [&](hegel::TestCase& tc) {
            int iters = 0;
            tc.repeat([&] {
                (void)tc.draw(gs::integers<int>());
                ++iters;
            });
            // Reached only because repeat() returns after the loop.
            returned = true;
            if (iters > max_iters) {
                max_iters = iters;
            }
        },
        hegel::Settings{.test_cases = 100,
                        .derandomize = true,
                        .database = hegel::Database::disabled()});
    EXPECT_TRUE(returned);
    EXPECT_GT(max_iters, 1) << "repeat() never iterated more than once";
}

// A failure raised inside a repeat() iteration is reported like any other
// failing property.
TEST(Loop, FailureInsideIsReported) {
    try {
        hegel::test(
            [](hegel::TestCase& tc) {
                tc.repeat([&] {
                    (void)tc.draw(gs::integers<int>());
                    throw std::runtime_error("boom");
                });
            },
            hegel::Settings{.test_cases = 200,
                            .derandomize = true,
                            .database = hegel::Database::disabled()});
        FAIL() << "expected the failure inside repeat() to be re-raised";
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "boom");
    }
}

// reject() inside an iteration discards that iteration and the loop continues;
// the run completes without reporting a failure.
TEST(Loop, RejectInsideContinues) {
    bool completed = false;
    hegel::test(
        [&](hegel::TestCase& tc) {
            tc.repeat([&] {
                int n = tc.draw(gs::integers<int>());
                if (n % 2 != 0) {
                    tc.reject();
                }
            });
            completed = true;
        },
        hegel::Settings{
            .test_cases = 100,
            .derandomize = true,
            .database = hegel::Database::disabled(),
            .suppress_health_check = {hegel::HealthCheck::FilterTooMuch}});
    EXPECT_TRUE(completed);
}
