#include <gtest/gtest.h>
#include <hegel/hegel.h>

#include <ApprovalTests.hpp>

#include "common/approvals.h"

namespace gs = hegel::generators;

using ApprovalTests::Approvals;
using hegel::tests::common::scrub_report;

TEST(Pools, PoolsRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        hegel::stateful::Pool<int> pool = hegel::stateful::Pool<int>(tc);
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = 10}));

        for (int num : original_set) {
            pool.add(num);
        }

        std::set<int> returned_set;
        for (int i = 0; i < original_set.size(); i++) {
            returned_set.insert(
                tc.draw(hegel::stateful::values_consumed(pool)));
        }

        assert(original_set == returned_set);
    });
}

TEST(Pools, PoolsNoConsume) {
    hegel::test([](hegel::TestCase& tc) {
        hegel::stateful::Pool<int> pool = hegel::stateful::Pool<int>(tc);
        uint8_t sz = tc.draw(gs::integers<uint8_t>());
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = sz}));

        for (int num : original_set) {
            pool.add(num);
        }

        for (int i = 0; i < original_set.size(); i++) {
            tc.draw(hegel::stateful::values_reusable(pool));
        }

        assert(pool.size() == original_set.size());
    });
}

TEST(Pools, DrawFromEmptyPool) {
    hegel::test([](hegel::TestCase& tc) {
        hegel::stateful::Pool<int> pool = hegel::stateful::Pool<int>(tc);
        tc.draw(hegel::stateful::values_reusable(pool));
        // should not error just a test case rejection
    });
}

namespace {
    struct Stack : hegel::stateful::StateMachine<Stack, std::vector<int>> {
        Stack() : StateMachine({.initial_state = {}}) {}
        std::vector<hegel::stateful::Rule<Stack>> rules() {
            return {
                hegel::stateful::Rule<Stack>("push",
                                             [](hegel::TestCase& tc, Stack& m) {
                                                 m.state.push_back(tc.draw(
                                                     gs::integers<int>()));
                                             }),
                hegel::stateful::Rule<Stack>("pop",
                                             [](hegel::TestCase& tc, Stack& m) {
                                                 tc.assume(!m.state.empty());
                                                 m.state.pop_back();
                                             }),
            };
        }
    };

    struct Empty : hegel::stateful::StateMachine<Empty, int> {
        Empty() : StateMachine({.initial_state = 0}) {}
        std::vector<hegel::stateful::Rule<Empty>> rules() { return {}; }
    };

    struct BoundedCounter : hegel::stateful::StateMachine<BoundedCounter, int> {
        BoundedCounter() : StateMachine({.initial_state = 0}) {}
        std::vector<hegel::stateful::Rule<BoundedCounter>> rules() {
            return {hegel::stateful::Rule<BoundedCounter>(
                "inc",
                [](hegel::TestCase&, BoundedCounter& m) { m.state += 1; })};
        }
        std::vector<hegel::stateful::Invariant<BoundedCounter>> invariants() {
            return {hegel::stateful::Invariant<BoundedCounter>(
                "bounded", [](const BoundedCounter& m) {
                    if (m.state >= 2) {
                        throw std::runtime_error("bound violated");
                    }
                })};
        }
    };

    struct StoppingCounter
        : hegel::stateful::StateMachine<StoppingCounter, int> {
        StoppingCounter() : StateMachine({.initial_state = 0}) {}
        std::vector<hegel::stateful::Rule<StoppingCounter>> rules() {
            return {hegel::stateful::Rule<StoppingCounter>(
                "inc", [](hegel::TestCase&, StoppingCounter& m) {
                    m.state += 1;
                    if (m.state >= 100) {
                        throw std::runtime_error("done");
                    }
                })};
        }
    };

    // The live set is the state. The pool is a member instead: it is tied to
    // a test case, and can be neither copied nor rendered.
    struct Allocator : hegel::stateful::StateMachine<Allocator, std::set<int>> {
        hegel::stateful::Pool<int> handles;
        int next_handle = 0;

        explicit Allocator(hegel::TestCase& tc)
            : StateMachine({.initial_state = {}}), handles(tc) {}

        std::vector<hegel::stateful::Rule<Allocator>> rules() {
            return {
                hegel::stateful::Rule<Allocator>(
                    "alloc",
                    [](hegel::TestCase&, Allocator& m) {
                        int h = m.next_handle++;
                        m.handles.add(h);
                        m.state.insert(h);
                    }),
                hegel::stateful::Rule<Allocator>(
                    "free",
                    [](hegel::TestCase& tc, Allocator& m) {
                        int h = tc.draw(
                            hegel::stateful::values_consumed(m.handles));
                        m.state.erase(h);
                    }),
            };
        }
        std::vector<hegel::stateful::Invariant<Allocator>> invariants() {
            return {hegel::stateful::Invariant<Allocator>(
                "sizes_agree", [](const Allocator& m) {
                    if (m.handles.size() != m.state.size()) {
                        throw std::runtime_error("pool and live set diverged");
                    }
                })};
        }
    };

    struct Adder : hegel::stateful::StateMachine<Adder, int> {
        Adder() : StateMachine({.initial_state = 0}) {}
        std::vector<hegel::stateful::Rule<Adder>> rules() {
            return {hegel::stateful::Rule<Adder>(
                "step", [](hegel::TestCase& tc, Adder& m) {
                    m.state += tc.draw(
                        "amount",
                        gs::integers<int>({.min_value = 1, .max_value = 9}));
                })};
        }
    };

    // Overflows at 12, so a failing sequence is short enough to read whole.
    struct Counter : hegel::stateful::StateMachine<Counter, int> {
        Counter() : StateMachine({.initial_state = 0}) {}
        std::vector<hegel::stateful::Rule<Counter>> rules() {
            return {hegel::stateful::Rule<Counter>(
                "add", [](hegel::TestCase& tc, Counter& m) {
                    m.state += tc.draw(
                        "amount",
                        gs::integers<int>({.min_value = 1, .max_value = 9}));
                    if (m.state >= 12) {
                        throw std::runtime_error("counter overflowed");
                    }
                })};
        }
    };

    std::string
    capture_counter_failure(hegel::stateful::RunParams params = {}) {
        testing::internal::CaptureStderr();
        try {
            hegel::test(
                [params](hegel::TestCase& tc) {
                    Counter machine;
                    hegel::stateful::run(machine, tc, params);
                },
                hegel::Settings{.seed = 1,
                                .derandomize = false,
                                .database = hegel::Database::disabled(),
                                .stateful_step_count = 5});
        } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
            // The report is what this asserts on; the re-raise is not.
        }
        return testing::internal::GetCapturedStderr();
    }
} // namespace

TEST(Stateful, BasicRun) {
    hegel::test([](hegel::TestCase& tc) {
        Stack machine;
        hegel::stateful::run(machine, tc);
    });
}

TEST(Stateful, EmptyRulesThrows) {
    EXPECT_THROW(hegel::test([](hegel::TestCase& tc) {
                     Empty machine;
                     hegel::stateful::run(machine, tc);
                 }),
                 std::invalid_argument);
}

TEST(Stateful, InvariantViolationReported) {
    EXPECT_THROW(hegel::test([](hegel::TestCase& tc) {
                     BoundedCounter machine;
                     hegel::stateful::run(machine, tc);
                 }),
                 std::runtime_error);
}

TEST(Stateful, SingleModeRunsUntilRuleStops) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         StoppingCounter machine;
                         hegel::stateful::run(machine, tc);
                     },
                     hegel::Settings{.database = hegel::Database::disabled(),
                                     .mode = hegel::Mode::SingleTestCase}),
                 std::runtime_error);
}

// The engine stops each case after at most stateful_step_count steps, so
// a rule that fails only on step 100 never gets there.
TEST(Stateful, StepCountCapsStepsPerCase) {
    hegel::test(
        [](hegel::TestCase& tc) {
            StoppingCounter machine;
            hegel::stateful::run(machine, tc);
        },
        hegel::Settings{.database = hegel::Database::disabled(),
                        .stateful_step_count = 5});
}

TEST(Stateful, StepCountBelowOneThrows) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         Stack machine;
                         hegel::stateful::run(machine, tc);
                     },
                     hegel::Settings{.database = hegel::Database::disabled(),
                                     .stateful_step_count = 0}),
                 std::runtime_error);
}

TEST(Stateful, PoolAsState) {
    hegel::test([](hegel::TestCase& tc) {
        Allocator machine(tc);
        hegel::stateful::run(machine, tc);
    });
}

TEST(Stateful, VerboseNestsDrawsAndHidesStopDecision) {
    testing::internal::CaptureStderr();
    hegel::test(
        [](hegel::TestCase& tc) {
            Adder machine;
            hegel::stateful::run(machine, tc);
        },
        hegel::Settings{.test_cases = 1,
                        .verbosity = hegel::Verbosity::Verbose,
                        .seed = 1,
                        .derandomize = false,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 3});
    std::string out = testing::internal::GetCapturedStderr();
    Approvals::verify(out, scrub_report());
}

// The state prints before the first step and after every step that runs to
// completion, so a failing sequence shows what led to the failure. A step
// that throws prints no state, leaving the last state before the exception
// as the one the failing step started from.
TEST(Stateful, StatePrintingIsOnByDefault) {
    Approvals::verify(capture_counter_failure(), scrub_report());
}

TEST(Stateful, StatePrintingCanBeDisabled) {
    Approvals::verify(capture_counter_failure({.print_state = false}),
                      scrub_report());
}
