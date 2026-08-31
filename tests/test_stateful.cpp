#include <gtest/gtest.h>
#include <hegel/hegel.h>

#include <ApprovalTests.hpp>

#include <atomic>
#include <mutex>

#include "common/approvals.h"

namespace gs = hegel::generators;
namespace hs = hegel::stateful;

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

TEST(ConcurrentPools, PoolsRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        hs::ConcurrentPool<int64_t> pool(tc);
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = 10}));

        for (int num : original_set) {
            int64_t value = num;
            pool.add(tc, value);
        }

        std::set<int64_t> expected(original_set.begin(), original_set.end());
        std::set<int64_t> returned;
        for (size_t i = 0; i < original_set.size(); i++) {
            returned.insert(tc.draw(hs::values_consumed(pool)));
        }

        assert(returned == expected);
    });
}

TEST(ConcurrentPools, PoolsNoConsume) {
    hegel::test([](hegel::TestCase& tc) {
        hs::ConcurrentPool<int64_t> pool(tc);
        uint8_t sz = tc.draw(gs::integers<uint8_t>());
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = sz}));

        for (int num : original_set) {
            pool.add(tc, static_cast<int64_t>(num));
        }

        for (size_t i = 0; i < original_set.size(); i++) {
            tc.draw(hs::values_reusable(pool));
        }

        assert(pool.size() == original_set.size());
    });
}

TEST(ConcurrentPools, DrawFromEmptyPool) {
    hegel::test([](hegel::TestCase& tc) {
        hs::ConcurrentPool<int64_t> pool(tc);
        tc.draw(hs::values_reusable(pool));
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

namespace {
    struct ConcurrentCounter : hs::ConcurrentStateMachine<ConcurrentCounter> {
        std::atomic<int64_t> value{0};

        std::vector<hs::ConcurrentRule<ConcurrentCounter>> rules() {
            return {
                hs::ConcurrentRule<ConcurrentCounter>(
                    "increment",
                    [](hegel::TestCase&, ConcurrentCounter& m) {
                        m.value.fetch_add(1, std::memory_order_seq_cst);
                    }),
                hs::ConcurrentRule<ConcurrentCounter>(
                    "decrement",
                    [](hegel::TestCase& tc, ConcurrentCounter& m) {
                        tc.assume(m.value.load(std::memory_order_seq_cst) > 0);
                        m.value.fetch_sub(1, std::memory_order_seq_cst);
                    }),
            };
        }

        std::vector<hs::ConcurrentInvariant<ConcurrentCounter>> invariants() {
            return {hs::ConcurrentInvariant<ConcurrentCounter>(
                "non_negative", [](const ConcurrentCounter& m) {
                    if (m.value.load(std::memory_order_seq_cst) < 0) {
                        throw std::runtime_error("counter became negative");
                    }
                })};
        }
    };

    struct Grouped : hs::ConcurrentStateMachine<Grouped> {
        mutable std::mutex mutex;
        std::vector<std::string> log;

        std::vector<hs::ConcurrentRule<Grouped>> rules() {
            return {
                hs::ConcurrentRule<Grouped>(
                    "alpha", "letters",
                    [](hegel::TestCase&, Grouped& m) {
                        std::lock_guard<std::mutex> lock(m.mutex);
                        m.log.push_back("alpha");
                    }),
                hs::ConcurrentRule<Grouped>(
                    "beta", "letters",
                    [](hegel::TestCase& tc, Grouped& m) {
                        int8_t n = tc.draw(gs::integers<int8_t>());
                        tc.assume(n != 0);
                        std::lock_guard<std::mutex> lock(m.mutex);
                        m.log.push_back("beta");
                    }),
                hs::ConcurrentRule<Grouped>(
                    "one", "numbers",
                    [](hegel::TestCase&, Grouped& m) {
                        std::lock_guard<std::mutex> lock(m.mutex);
                        m.log.push_back("one");
                    }),
                hs::ConcurrentRule<Grouped>(
                    "anonymous",
                    [](hegel::TestCase&, Grouped& m) {
                        std::lock_guard<std::mutex> lock(m.mutex);
                        m.log.push_back("anonymous");
                    }),
            };
        }

        std::vector<hs::ConcurrentInvariant<Grouped>> invariants() {
            return {hs::ConcurrentInvariant<Grouped>(
                "log_is_bounded", [](const Grouped& m) {
                    std::lock_guard<std::mutex> lock(m.mutex);
                    if (m.log.size() > 100000) {
                        throw std::runtime_error("log grew without bound");
                    }
                })};
        }
    };

    struct PoolMachine : hs::ConcurrentStateMachine<PoolMachine> {
        hs::ConcurrentPool<int64_t> pool;
        std::atomic<int64_t> next{0};

        explicit PoolMachine(hegel::TestCase& tc) : pool(tc) {}

        std::vector<hs::ConcurrentRule<PoolMachine>> rules() {
            return {
                hs::ConcurrentRule<PoolMachine>(
                    "add",
                    [](hegel::TestCase& tc, PoolMachine& m) {
                        m.pool.add(
                            tc, m.next.fetch_add(1, std::memory_order_seq_cst));
                    }),
                hs::ConcurrentRule<PoolMachine>(
                    "reuse",
                    [](hegel::TestCase& tc, PoolMachine& m) {
                        int64_t value = tc.draw(hs::values_reusable(m.pool));
                        if (value < 0) {
                            throw std::runtime_error("negative pooled value");
                        }
                    }),
                hs::ConcurrentRule<PoolMachine>(
                    "consume",
                    [](hegel::TestCase& tc, PoolMachine& m) {
                        int64_t value = tc.draw(hs::values_consumed(m.pool));
                        if (value < 0) {
                            throw std::runtime_error("negative pooled value");
                        }
                    }),
            };
        }

        std::vector<hs::ConcurrentInvariant<PoolMachine>> invariants() {
            return {hs::ConcurrentInvariant<PoolMachine>(
                "pool_is_bounded", [](const PoolMachine& m) {
                    if (m.pool.size() > 100000) {
                        throw std::runtime_error("pool grew without bound");
                    }
                })};
        }
    };

    struct Boom : hs::ConcurrentStateMachine<Boom> {
        std::vector<hs::ConcurrentRule<Boom>> rules() {
            return {hs::ConcurrentRule<Boom>("boom", [](hegel::TestCase& tc,
                                                        Boom&) {
                int64_t value = tc.draw(hegel::generators::integers<int64_t>());
                throw std::runtime_error("concurrent boom " +
                                         std::to_string(value));
            })};
        }
    };

    struct NoRules : hs::ConcurrentStateMachine<NoRules> {
        std::vector<hs::ConcurrentRule<NoRules>> rules() { return {}; }
    };
} // namespace
