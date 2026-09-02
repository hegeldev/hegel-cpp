#include <gtest/gtest.h>
#include <hegel/hegel.h>

#include <ApprovalTests.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "common/approvals.h"

namespace gs = hegel::generators;
namespace hs = hegel::stateful;

using ApprovalTests::Approvals;
using hegel::tests::common::scrub_concurrent_report;
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

    struct SampledInvariantMachine
        : hs::StateMachine<SampledInvariantMachine, int> {
        std::shared_ptr<std::atomic<int64_t>> rules_run;
        std::shared_ptr<std::atomic<int64_t>> invariants_run;

        SampledInvariantMachine(
            std::shared_ptr<std::atomic<int64_t>> rules_run,
            std::shared_ptr<std::atomic<int64_t>> invariants_run)
            : StateMachine({.initial_state = 0}),
              rules_run(std::move(rules_run)),
              invariants_run(std::move(invariants_run)) {}

        std::vector<hs::Rule<SampledInvariantMachine>> rules() {
            return {hs::Rule<SampledInvariantMachine>(
                "count_rule", [](hegel::TestCase&, SampledInvariantMachine& m) {
                    m.rules_run->fetch_add(1, std::memory_order_relaxed);
                })};
        }

        std::vector<hs::Invariant<SampledInvariantMachine>> invariants() {
            return {hs::Invariant<SampledInvariantMachine>(
                "count_invariant", [](const SampledInvariantMachine& m) {
                    m.invariants_run->fetch_add(1, std::memory_order_relaxed);
                })};
        }
    };

    struct BreakOnceMachine : hs::StateMachine<BreakOnceMachine, bool> {
        BreakOnceMachine() : StateMachine({.initial_state = false}) {}

        std::vector<hs::Rule<BreakOnceMachine>> rules() {
            return {hs::Rule<BreakOnceMachine>(
                "break_it",
                [](hegel::TestCase&, BreakOnceMachine& m) { m.state = true; })};
        }

        std::vector<hs::Invariant<BreakOnceMachine>> invariants() {
            return {hs::Invariant<BreakOnceMachine>(
                "not_broken", [](const BreakOnceMachine& m) {
                    if (m.state) {
                        throw std::runtime_error("machine is broken");
                    }
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

// Mode::SingleTestCase runs one bounded case: each case still stops after at
// most stateful_step_count steps, so StoppingCounter (which throws only on
// step 100) never reaches its threshold and the run passes.
TEST(Stateful, SingleModeRunsOneBoundedCase) {
    EXPECT_NO_THROW(hegel::test(
        [](hegel::TestCase& tc) {
            StoppingCounter machine;
            hegel::stateful::run(machine, tc);
        },
        hegel::Settings{.database = hegel::Database::disabled(),
                        .mode = hegel::Mode::SingleTestCase}));
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

TEST(Stateful, InvariantsAreSampledRatherThanRunAfterEveryRule) {
    auto rules_run = std::make_shared<std::atomic<int64_t>>(0);
    auto invariants_run = std::make_shared<std::atomic<int64_t>>(0);
    hegel::test(
        [rules_run, invariants_run](hegel::TestCase& tc) {
            SampledInvariantMachine machine(rules_run, invariants_run);
            hs::run(machine, tc);
        },
        hegel::Settings{.test_cases = 20,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 50});

    int64_t rule_count = rules_run->load(std::memory_order_relaxed);
    int64_t invariant_count = invariants_run->load(std::memory_order_relaxed);
    EXPECT_GE(invariant_count, 2);
    EXPECT_LT(invariant_count, rule_count / 4)
        << "expected sampled invariant runs (" << invariant_count
        << ") to stay far below rule runs (" << rule_count << ")";
}

TEST(Stateful, PersistentViolationsAreAlwaysCaughtDespiteSampling) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         BreakOnceMachine machine;
                         hs::run(machine, tc);
                     },
                     hegel::Settings{.database = hegel::Database::disabled()}),
                 std::runtime_error);
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

        std::vector<hs::Invariant<ConcurrentCounter>> invariants() {
            return {hs::Invariant<ConcurrentCounter>(
                "non_negative", [](const ConcurrentCounter& m) {
                    EXPECT_GE(m.value.load(std::memory_order_seq_cst), 0);
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

        std::vector<hs::Invariant<Grouped>> invariants() {
            return {
                hs::Invariant<Grouped>("log_is_bounded", [](const Grouped& m) {
                    std::lock_guard<std::mutex> lock(m.mutex);
                    EXPECT_LE(m.log.size(), 100000);
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
                        EXPECT_GE(value, 0);
                    }),
                hs::ConcurrentRule<PoolMachine>(
                    "consume",
                    [](hegel::TestCase& tc, PoolMachine& m) {
                        int64_t value = tc.draw(hs::values_consumed(m.pool));
                        EXPECT_GE(value, 0);
                    }),
            };
        }

        std::vector<hs::Invariant<PoolMachine>> invariants() {
            return {hs::Invariant<PoolMachine>(
                "pool_is_bounded", [](const PoolMachine& m) {
                    EXPECT_LE(m.pool.size(), 100000);
                })};
        }
    };

    struct ConcurrentSampledInvariantMachine
        : hs::ConcurrentStateMachine<ConcurrentSampledInvariantMachine> {
        std::shared_ptr<std::atomic<int64_t>> rules_run;
        std::shared_ptr<std::atomic<int64_t>> invariants_run;

        ConcurrentSampledInvariantMachine(
            std::shared_ptr<std::atomic<int64_t>> rules_run,
            std::shared_ptr<std::atomic<int64_t>> invariants_run)
            : rules_run(std::move(rules_run)),
              invariants_run(std::move(invariants_run)) {}

        std::vector<hs::ConcurrentRule<ConcurrentSampledInvariantMachine>>
        rules() {
            return {hs::ConcurrentRule<ConcurrentSampledInvariantMachine>(
                "count_rule",
                [](hegel::TestCase&, ConcurrentSampledInvariantMachine& m) {
                    m.rules_run->fetch_add(1, std::memory_order_relaxed);
                })};
        }

        std::vector<hs::Invariant<ConcurrentSampledInvariantMachine>>
        invariants() {
            return {hs::Invariant<ConcurrentSampledInvariantMachine>(
                "count_invariant",
                [](const ConcurrentSampledInvariantMachine& m) {
                    m.invariants_run->fetch_add(1, std::memory_order_relaxed);
                })};
        }
    };

    struct Boom : hs::ConcurrentStateMachine<Boom> {
        std::vector<hs::ConcurrentRule<Boom>> rules() {
            return {hs::ConcurrentRule<Boom>(
                "boom", [](hegel::TestCase& tc, Boom&) {
                    int value = tc.draw(hegel::generators::integers<int>());
                    throw std::runtime_error("concurrent boom " +
                                             std::to_string(value));
                })};
        }
    };

    struct RejectsEveryRule : hs::ConcurrentStateMachine<RejectsEveryRule> {
        std::vector<hs::ConcurrentRule<RejectsEveryRule>> rules() {
            return {hs::ConcurrentRule<RejectsEveryRule>(
                "reject", [](hegel::TestCase& tc, RejectsEveryRule&) {
                    tc.assume(false);
                })};
        }
    };

    struct ThrowsInvalidArgument
        : hs::ConcurrentStateMachine<ThrowsInvalidArgument> {
        std::vector<hs::ConcurrentRule<ThrowsInvalidArgument>> rules() {
            return {hs::ConcurrentRule<ThrowsInvalidArgument>(
                "throw", [](hegel::TestCase&, ThrowsInvalidArgument&) {
                    throw std::invalid_argument("invalid rule");
                })};
        }
    };

    struct ThrowsNonStandard : hs::ConcurrentStateMachine<ThrowsNonStandard> {
        std::vector<hs::ConcurrentRule<ThrowsNonStandard>> rules() {
            return {hs::ConcurrentRule<ThrowsNonStandard>(
                "throw",
                [](hegel::TestCase&, ThrowsNonStandard&) { throw 42; })};
        }
    };

    struct NoRules : hs::ConcurrentStateMachine<NoRules> {
        std::vector<hs::ConcurrentRule<NoRules>> rules() { return {}; }
    };
} // namespace

TEST(ConcurrentStateful, FixedSingleWorkerPasses) {
    hegel::test(
        [](hegel::TestCase& tc) {
            ConcurrentCounter machine;
            hs::run_concurrent(machine, tc, 1, 1);
        },
        hegel::Settings{.test_cases = 5,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 5});
}

TEST(ConcurrentStateful, GroupedMachinePasses) {
    hegel::test(
        [](hegel::TestCase& tc) {
            Grouped machine;
            hs::run_concurrent(machine, tc, 1, 3);
        },
        hegel::Settings{.test_cases = 10,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 10});
}

TEST(ConcurrentStateful, InvariantsAreSampledRatherThanRunAfterEveryRound) {
    auto rules_run = std::make_shared<std::atomic<int64_t>>(0);
    auto invariants_run = std::make_shared<std::atomic<int64_t>>(0);
    hegel::test(
        [rules_run, invariants_run](hegel::TestCase& tc) {
            ConcurrentSampledInvariantMachine machine(rules_run,
                                                      invariants_run);
            hs::run_concurrent(machine, tc, 1, 1);
        },
        hegel::Settings{.test_cases = 20,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 50});

    int64_t rule_count = rules_run->load(std::memory_order_relaxed);
    int64_t invariant_count = invariants_run->load(std::memory_order_relaxed);
    EXPECT_GE(invariant_count, 2);
    EXPECT_LT(invariant_count, rule_count / 4)
        << "expected sampled invariant runs (" << invariant_count
        << ") to stay far below rule runs (" << rule_count << ")";
}

TEST(ConcurrentPools, AddReuseAndConsume) {
    hegel::test([](hegel::TestCase& tc) {
        hs::ConcurrentPool<int64_t> pool(tc);
        EXPECT_TRUE(pool.empty());
        pool.add(tc, 10);
        pool.add(tc, 20);
        EXPECT_EQ(pool.size(), 2);
        int64_t reused = tc.draw(hs::values_reusable(pool));
        EXPECT_TRUE(reused == 10 || reused == 20);
        EXPECT_EQ(pool.size(), 2);
        int64_t first = tc.draw(hs::values_consumed(pool));
        EXPECT_EQ(pool.size(), 1);
        int64_t second = tc.draw(hs::values_consumed(pool));
        EXPECT_EQ(first + second, 30);
        EXPECT_TRUE(pool.empty());
    });
}

TEST(ConcurrentPools, SharedAcrossWorkers) {
    hegel::test(
        [](hegel::TestCase& tc) {
            PoolMachine machine(tc);
            hs::run_concurrent(machine, tc, 1, 3);
        },
        hegel::Settings{.test_cases = 10,
                        .database = hegel::Database::disabled(),
                        .stateful_step_count = 10});
}

TEST(ConcurrentStateful, WorkerExceptionHasOriginAndBufferedOutput) {
    testing::internal::CaptureStderr();
    std::string message;
    try {
        hegel::test(
            [](hegel::TestCase& tc) {
                Boom machine;
                hs::run_concurrent(machine, tc, 2, 2);
            },
            hegel::Settings{.print_blob = true,
                            .database = hegel::Database::disabled()});
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    std::string output = testing::internal::GetCapturedStderr();

    EXPECT_NE(message.find("concurrent boom"), std::string::npos) << message;
    EXPECT_NE(output.find("Concurrent state machine detected"),
              std::string::npos)
        << output;
    EXPECT_NE(output.find("---------------- Round 1: group \"<anonymous>\" "
                          "----------------"),
              std::string::npos)
        << output;

    std::smatch rule_match;
    std::regex rule_pattern(
        R"(\[worker ([0-9]+) \+[0-9]+\.[0-9]{3}ms\] Rule: boom)");
    ASSERT_TRUE(std::regex_search(output, rule_match, rule_pattern)) << output;
    std::string worker = rule_match[1].str();
    std::regex draw_pattern("\\[worker " + worker +
                            " \\+[0-9]+\\.[0-9]{3}ms\\]   auto "
                            "draw_[0-9]+ =");
    EXPECT_TRUE(std::regex_search(output, draw_pattern)) << output;
    EXPECT_NE(output.find("Exception: std::runtime_error: concurrent boom"),
              std::string::npos)
        << output;
    EXPECT_EQ(output.find("<unknown>"), std::string::npos) << output;
    EXPECT_EQ(output.find("rerun with:"), std::string::npos) << output;
}

TEST(ConcurrentStateful, FixedSingleWorkerFailureRemainsDeterministic) {
    testing::internal::CaptureStderr();
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         Boom machine;
                         hs::run_concurrent(machine, tc, 1, 1);
                     },
                     hegel::Settings{.print_blob = true,
                                     .database = hegel::Database::disabled()}),
                 std::runtime_error);
    std::string output = testing::internal::GetCapturedStderr();
    Approvals::verify(output, scrub_concurrent_report());
}

TEST(ConcurrentStateful, AllRejectedRulesAreDiscarded) {
    hegel::test(
        [](hegel::TestCase& tc) {
            RejectsEveryRule machine;
            hs::run_concurrent(machine, tc, 1, 1);
        },
        hegel::Settings{.test_cases = 1,
                        .verbosity = hegel::Verbosity::Quiet,
                        .database = hegel::Database::disabled()});
}

TEST(ConcurrentStateful, InvalidArgumentFromWorkerIsRethrown) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         ThrowsInvalidArgument machine;
                         hs::run_concurrent(machine, tc, 1, 1);
                     },
                     hegel::Settings{.verbosity = hegel::Verbosity::Quiet,
                                     .database = hegel::Database::disabled()}),
                 std::invalid_argument);
}

TEST(ConcurrentStateful, NonStandardExceptionFromWorkerIsRethrown) {
    EXPECT_ANY_THROW(hegel::test(
        [](hegel::TestCase& tc) {
            ThrowsNonStandard machine;
            hs::run_concurrent(machine, tc, 1, 1);
        },
        hegel::Settings{.verbosity = hegel::Verbosity::Quiet,
                        .database = hegel::Database::disabled()}));
}

TEST(ConcurrentStateful, InvalidConcurrencyBoundsAreRejected) {
    for (const auto& [minimum, maximum] :
         std::vector<std::pair<int64_t, int64_t>>{{0, 1}, {2, 1}}) {
        EXPECT_THROW(
            hegel::test(
                [=](hegel::TestCase& tc) {
                    ConcurrentCounter machine;
                    hs::run_concurrent(machine, tc, minimum, maximum);
                },
                hegel::Settings{.database = hegel::Database::disabled()}),
            std::invalid_argument);
    }
}

TEST(ConcurrentStateful, MachineWithoutRulesIsRejected) {
    EXPECT_THROW(hegel::test(
                     [](hegel::TestCase& tc) {
                         NoRules machine;
                         hs::run_concurrent(machine, tc, 1, 1);
                     },
                     hegel::Settings{.database = hegel::Database::disabled()}),
                 std::invalid_argument);
}
