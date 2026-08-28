// Test-case cloning: clone() forks an independent draw stream of the same test
// case; spawn()/join() run a clone on another thread and carry its result or
// exception back. The behavioral tests mirror hegel-ocaml's test_clone.ml; the
// CloneOutput suite pins the printed trace/counterexample of a run that clones.

#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <ApprovalTests.hpp>

#include <hegel/hegel.h>

#include "common/approvals.h"

using ApprovalTests::Approvals;
using hegel::tests::common::scrub_report;

namespace gs = hegel::generators;

namespace {
    // One test case, no shrinking, no database — the C++ analog of
    // test_clone.ml's `single_settings`.
    hegel::Settings single() {
        return {.database = hegel::Database::disabled(),
                .mode = hegel::Mode::SingleTestCase};
    }

    gs::Generator<int> small_int() {
        return gs::integers<int>({.min_value = 0, .max_value = 9});
    }
} // namespace

// ---------------------------------------------------------------------------
// Behavioral tests (value assertions), mirroring test_clone.ml
// ---------------------------------------------------------------------------

// A clone drawn inside the body produces a value; the parent stays usable
// alongside it.
TEST(Clone, DrawsAndParentStaysUsable) {
    int parent = -1;
    int cloned = -1;
    hegel::test(
        [&](hegel::TestCase& tc) {
            hegel::TestCase worker = tc.clone();
            cloned = worker.draw(small_int());
            parent = tc.draw(small_int());
        },
        single());
    EXPECT_GE(parent, 0);
    EXPECT_LE(parent, 9);
    EXPECT_GE(cloned, 0);
    EXPECT_LE(cloned, 9);
}

// Parent and clone can be driven from two threads at once without tripping the
// engine's concurrent-use guard.
TEST(Clone, ConcurrentTwoThreads) {
    int worker_value = -1;
    hegel::test(
        [&](hegel::TestCase& tc) {
            hegel::TestCase worker = tc.clone();
            std::thread t([&] { worker_value = worker.draw(small_int()); });
            int main_value = tc.draw(small_int());
            t.join();
            EXPECT_GE(main_value, 0);
            EXPECT_LE(main_value, 9);
        },
        single());
    EXPECT_GE(worker_value, 0);
    EXPECT_LE(worker_value, 9);
}

// With a fixed seed, the parent's and the clone's drawn values reproduce
// exactly across runs.
TEST(Clone, ReproducibleUnderSeed) {
    auto run = [] {
        std::pair<int, int> drawn{-1, -1};
        hegel::test(
            [&](hegel::TestCase& tc) {
                int p = tc.draw(small_int());
                int c = tc.clone().draw(small_int());
                drawn = {p, c};
            },
            hegel::Settings{.seed = 42,
                            .database = hegel::Database::disabled(),
                            .mode = hegel::Mode::SingleTestCase});
        return drawn;
    };
    EXPECT_EQ(run(), run());
}

// Cloning a clone yields a further independent stream that still draws.
TEST(Clone, CloneOfCloneDraws) {
    int value = -1;
    hegel::test(
        [&](hegel::TestCase& tc) {
            hegel::TestCase c1 = tc.clone();
            hegel::TestCase c2 = c1.clone();
            value = c2.draw(small_int());
        },
        single());
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 9);
}

// spawn()/join() runs the body on a clone in another thread and returns its
// value; the parent draws its own value meanwhile.
TEST(Clone, SpawnJoinReturnsValue) {
    int main_value = -1;
    int worker_value = -1;
    hegel::test(
        [&](hegel::TestCase& tc) {
            auto worker = tc.spawn(
                [](hegel::TestCase& w) { return w.draw(small_int()); });
            main_value = tc.draw(small_int());
            worker_value = worker.join();
        },
        single());
    EXPECT_GE(main_value, 0);
    EXPECT_LE(main_value, 9);
    EXPECT_GE(worker_value, 0);
    EXPECT_LE(worker_value, 9);
}

// join() re-raises an exception thrown on the worker thread; the parent handle
// is still usable afterward.
TEST(Clone, SpawnJoinReraises) {
    bool raised = false;
    hegel::test(
        [&](hegel::TestCase& tc) {
            auto worker = tc.spawn([](hegel::TestCase&) -> int {
                throw std::runtime_error("worker boom");
            });
            try {
                (void)worker.join();
            } catch (const std::runtime_error& e) {
                if (std::string(e.what()) == "worker boom") {
                    raised = true;
                }
            }
            (void)tc.draw(small_int());
        },
        single());
    EXPECT_TRUE(raised);
}

// Move-assigning a clone transfers its stream; the target releases its previous
// handle and the source becomes inert.
TEST(Clone, MoveAssignTransfersStream) {
    int value = -1;
    hegel::test(
        [&](hegel::TestCase& tc) {
            hegel::TestCase a = tc.clone();
            hegel::TestCase b = tc.clone();
            a = std::move(b);
            value = a.draw(small_int());
        },
        single());
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 9);
}

// A worker left un-joined is joined by its destructor rather than terminating.
// The worker records its draw before finishing, so a completed value proves the
// destructor joined a finished thread (the result is published under the join's
// happens-before edge).
TEST(Clone, WorkerDestructorJoinsIfNotJoined) {
    int worker_value = -1;
    hegel::test(
        [&](hegel::TestCase& tc) {
            auto worker = tc.spawn([&](hegel::TestCase& w) {
                worker_value = w.draw(small_int());
                return 0;
            });
            // Leaves scope without join(); the destructor joins the thread.
        },
        single());
    EXPECT_GE(worker_value, 0);
    EXPECT_LE(worker_value, 9);
}

// ---------------------------------------------------------------------------
// Clone output approval (snapshot) tests
// ---------------------------------------------------------------------------

namespace {
    constexpr const char* kNote = "SENTINEL_NOTE";

    // Runs a failing property with a fixed seed so the shrunk replay, and with
    // it the whole report, is deterministic.
    std::string
    capture_failure_report(const std::function<void(hegel::TestCase&)>& body,
                           hegel::Settings settings = {}) {
        settings.seed = 1;
        settings.derandomize = false;
        settings.database = hegel::Database::disabled();
        testing::internal::CaptureStderr();
        std::string rethrown = "<no exception>";
        try {
            hegel::test(body, settings);
        } catch (const std::exception& e) {
            rethrown = e.what();
        } catch (...) {
            rethrown = "<non-std exception>";
        }
        return testing::internal::GetCapturedStderr() +
               "--- rethrown: " + rethrown + "\n";
    }
} // namespace

// A counterexample drawn on a clone is recorded in the choice sequence and
// reproduced in the failure replay: the report shows the clone's shrunk draw
// (10) and the failure message.
TEST(CloneOutput, FailureReplayIncludesCloneDraw) {
    std::string out = capture_failure_report([](hegel::TestCase& tc) {
        hegel::TestCase worker = tc.clone();
        int32_t x = worker.draw(gs::integers<int32_t>());
        if (x >= 10) {
            throw std::runtime_error("clone failure with x=" +
                                     std::to_string(x));
        }
    });
    Approvals::verify(out, scrub_report());
}

// Verbose trace of a run that draws on the parent and then on a clone (both on
// one thread, so the trace is deterministic): each case prints the note and
// both drawn values.
TEST(CloneOutput, VerboseTraceShowsParentAndCloneDraws) {
    testing::internal::CaptureStderr();
    hegel::test(
        [](hegel::TestCase& tc) {
            tc.note(kNote);
            (void)tc.draw(gs::integers<int32_t>());
            (void)tc.clone().draw(gs::integers<int32_t>());
        },
        hegel::Settings{.test_cases = 3,
                        .verbosity = hegel::Verbosity::Verbose,
                        .seed = 1,
                        .derandomize = false,
                        .database = hegel::Database::disabled()});
    Approvals::verify(testing::internal::GetCapturedStderr());
}
