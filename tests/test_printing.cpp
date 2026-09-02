// Printing and failure reporting: the human-facing output Hegel emits — failure
// replays, blobs, multiple-failure reports (driven through the subject
// subprocess), and per-verbosity diagnostics (captured in-process).

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <regex>
#include <string>

#include <hegel/hegel.h>
#include <hegel/internal.h>

#include <ApprovalTests.hpp>

#include "common/subprocess.h"
#include "common/utils.h"

#include "common/approvals.h"

using ApprovalTests::Approvals;
using hegel::tests::common::assert_matches_regex;
using hegel::tests::common::run_subject;
using hegel::tests::common::scrub_blob;
using hegel::tests::common::scrub_report;
using hegel::tests::common::SubprocessResult;

#ifndef HEGEL_SUBJECT_BIN
#error "HEGEL_SUBJECT_BIN must be defined at build time"
#endif

namespace gs = hegel::generators;

namespace {
    // Run the prebuilt subject binary for one scenario (see subject_main.cpp).
    // No per-test recompile, so these tests run fast and in parallel.
    SubprocessResult run_scenario(const std::string& name) {
        return run_subject(HEGEL_SUBJECT_BIN, {name});
    }
} // namespace

TEST(Output, FailingScenariosExitNonZero) {
    for (const char* scenario :
         {"failing", "stable_origin", "throw_int", "throw_custom",
          "multiple_failures", "multiple_failures_off", "exception_message",
          "print_blob"}) {
        SubprocessResult r = run_scenario(scenario);
        EXPECT_NE(r.exit_code, 0) << scenario;
    }
}

// Two `throw`s of one type are two bugs in a fresh process as well, where
// the binary loads at a different address. A throw site holds an offset from
// its enclosing function, so it survives ASLR and repeats run to run.
TEST(Output, ThrowSitesGroupTheSameWayInEveryProcess) {
#ifdef HEGEL_TESTS_NO_THROW_SITE
    GTEST_SKIP() << "HEGEL_THROW_SITE=OFF compiles throw-site capture out";
#endif
    for (int run = 0; run < 3; run++) {
        SubprocessResult r = run_scenario("throw_sites");
        EXPECT_NE(r.exit_code, 0);
        assert_matches_regex(r.stderr_data, R"(Failure 1 of 2)");
        assert_matches_regex(r.stderr_data, R"(even bug with x=10)");
        assert_matches_regex(r.stderr_data, R"(odd bug with x=11)");
    }
}

TEST(Output, ExceptionMessageIsPrinted) {
    SubprocessResult r = run_scenario("exception_message");
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(custom exception for x=7)");
}

namespace {
    // A fixed seed keeps the shrunk replay, and with it the whole report,
    // deterministic.
    hegel::Settings report_settings(hegel::Settings settings = {}) {
        settings.seed = 1;
        settings.derandomize = false;
        settings.database = hegel::Database::disabled();
        return settings;
    }

    // Returns what one failing run printed, plus the exception it re-raised.
    std::string capture_report(const std::function<void()>& run) {
        testing::internal::CaptureStderr();
        std::string rethrown = "<no exception>";
        try {
            run();
        } catch (const std::exception& e) {
            rethrown = e.what();
        } catch (...) {
            rethrown = "<non-std exception>";
        }
        return testing::internal::GetCapturedStderr() +
               "--- rethrown: " + rethrown + "\n";
    }

    std::string
    capture_failure_report(const std::function<void(hegel::TestCase&)>& body,
                           hegel::Settings settings = {}) {
        return capture_report(
            [&] { hegel::test(body, report_settings(settings)); });
    }

    const hegel::TestLocation kLocation{"my_property", "tests/example.cpp", 42};

    void throw_boom(hegel::TestCase&) { throw std::runtime_error("boom"); }
} // namespace

// Fails on every case; the minimal counterexample is 0.
TEST(FailureReport, SingleFailure) {
    std::string out = capture_failure_report([](hegel::TestCase& tc) {
        int32_t x = tc.draw(gs::integers<int32_t>());
        throw std::runtime_error("intentional failure: " + std::to_string(x));
    });
    Approvals::verify(out, scrub_report());
}

// Fails for x >= 10; the replayed counterexample shrinks to exactly 10.
TEST(FailureReport, OriginStableAcrossDrawnValues) {
    std::string out = capture_failure_report([](hegel::TestCase& tc) {
        int32_t x = tc.draw(gs::integers<int32_t>());
        if (x >= 10) {
            throw std::runtime_error("failure with x=" + std::to_string(x));
        }
    });
    Approvals::verify(out, scrub_report());
}

// A non-std exception still produces the replay output before re-raising.
TEST(FailureReport, NonStdException) {
    std::string out = capture_failure_report([](hegel::TestCase& tc) {
        int32_t x = tc.draw(gs::integers<int32_t>());
        if (x >= 5) {
            throw 42;
        }
    });
    Approvals::verify(out, scrub_report());
}

// A body that throws before it draws anything has nothing to show between the
// count and the exception, so the report keeps no empty body block.
TEST(FailureReport, NoDrawsPrintsBodylessReport) {
    std::string out = capture_failure_report(throw_boom);
    Approvals::verify(out, scrub_report());
}

// A test location names the test and its source line in the header. The
// location overload takes no blobs, so naming a test costs one argument.
TEST(FailureReport, LocationNamesTheTest) {
    std::string out = capture_report(
        [] { hegel::test(throw_boom, kLocation, report_settings()); });
    Approvals::verify(out, scrub_blob());
}

// HEGEL_TEST goes through its own entry point, which prints the annotation
// form of the rerun hint. HEGEL_REPRODUCE_FAILURE only applies to a test that
// macro defined, so every other caller keeps the plain form above — the same
// location through hegel::test() proves the two are independent.
TEST(FailureReport, MacroDefinedTestGetsAnnotationRerunHint) {
    std::string out = capture_report([] {
        hegel::internal::test_from_macro(throw_boom, kLocation,
                                         report_settings(), {});
    });
    Approvals::verify(out, scrub_blob());
}

TEST(FailureReport, DiscardedCasesAreCounted) {
    int calls = 0;
    std::string out = capture_failure_report([&calls](hegel::TestCase& tc) {
        (void)tc.draw(gs::integers<int32_t>());
        if (++calls <= 3) {
            tc.reject();
        }
        throw std::runtime_error("boom");
    });
    Approvals::verify(out, scrub_report());
}

// A note that spans several lines keeps every line inside the body's
// indent.
TEST(FailureReport, MultiLineNoteIsIndented) {
    std::string out = capture_failure_report([](hegel::TestCase& tc) {
        tc.note("first line\nsecond line");
        throw std::runtime_error("boom");
    });
    Approvals::verify(out, scrub_report());
}

namespace {
    std::string capture_two_throw_sites_report() {
        return capture_failure_report(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 10 && x % 2 == 0) {
                    throw std::runtime_error("even bug with x=" +
                                             std::to_string(x));
                }
                if (x >= 10 && x % 2 != 0) {
                    throw std::runtime_error("odd bug with x=" +
                                             std::to_string(x));
                }
            },
            hegel::Settings{.report_multiple_failures = true});
    }

    class Tagged : public std::runtime_error, public hegel::FailureOrigin {
      public:
        Tagged(std::string tag, const std::string& message)
            : std::runtime_error(message), tag_(std::move(tag)) {}
        std::string failure_origin() const override { return tag_; }

      private:
        std::string tag_;
    };
} // namespace

#ifdef HEGEL_TESTS_NO_THROW_SITE
// Without throw-site capture, throws of one type collapse into one failure.
TEST(FailureReport, ThrowSitesDisabled) {
    Approvals::verify(capture_two_throw_sites_report(), scrub_report());
}
#else
// With throw-site capture, the two sites are distinct origins
TEST(FailureReport, ThrowSitesEnabled) {
    Approvals::verify(capture_two_throw_sites_report(), scrub_report());
}
#endif

// A site holds no generated values, so every value that reaches one `throw`
// is the same bug.
TEST(FailureReport, OneThrowSiteIsOneBug) {
    std::string out = capture_failure_report(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 10) {
                throw std::runtime_error("bug with x=" + std::to_string(x));
            }
        },
        hegel::Settings{.report_multiple_failures = true});
    EXPECT_TRUE(out.find("Failure 1 of") == std::string::npos) << out;
}

// An exception that names its own origin splits one `throw` into two bugs,
// which its site alone cannot do.
TEST(FailureReport, FailureOriginSplitsOneThrowSite) {
    std::string out = capture_failure_report(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 10) {
                throw Tagged(x % 2 == 0 ? "even" : "odd",
                             "bug with x=" + std::to_string(x));
            }
        },
        hegel::Settings{.report_multiple_failures = true});
    EXPECT_TRUE(out.find("Failure 1 of 2") != std::string::npos) << out;
    EXPECT_TRUE(out.find("bug with x=10") != std::string::npos) << out;
    EXPECT_TRUE(out.find("bug with x=11") != std::string::npos) << out;
}

// It joins two `throw`s into one bug as well, so the origin it names is the
// whole of what Hegel groups by.
TEST(FailureReport, FailureOriginJoinsTwoThrowSites) {
    std::string out = capture_failure_report(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 10 && x % 2 == 0) {
                throw Tagged("one", "even bug with x=" + std::to_string(x));
            }
            if (x >= 10 && x % 2 != 0) {
                throw Tagged("one", "odd bug with x=" + std::to_string(x));
            }
        },
        hegel::Settings{.report_multiple_failures = true});
    EXPECT_TRUE(out.find("Failure 1 of") == std::string::npos) << out;
}

namespace {
    // Two distinct bugs (different exception types, so different origins):
    // even x >= 10 shrinks to 10, odd x >= 10 shrinks to 11.
    void two_bugs(hegel::TestCase& tc) {
        int32_t x = tc.draw(gs::integers<int32_t>());
        if (x >= 10 && x % 2 == 0) {
            throw std::runtime_error("even bug with x=" + std::to_string(x));
        }
        if (x >= 10 && x % 2 != 0) {
            throw std::logic_error("odd bug with x=" + std::to_string(x));
        }
    }
} // namespace

TEST(FailureReport, MultipleFailuresReported) {
    std::string out = capture_failure_report(
        two_bugs, hegel::Settings{.report_multiple_failures = true});
    Approvals::verify(out, scrub_report());
}

// With report_multiple_failures off, the run stops at the first failing
// example and the report holds a single failure.
TEST(FailureReport, MultipleFailuresOffReportsOne) {
    std::string out = capture_failure_report(
        two_bugs, hegel::Settings{.report_multiple_failures = false});
    Approvals::verify(out, scrub_report());
}

// print_blob = false drops the rerun hint. The rest of the report stays.
TEST(FailureReport, PrintBlobOffDropsRerunHint) {
    std::string out = capture_failure_report(
        [](hegel::TestCase&) { throw std::runtime_error("silly error"); },
        hegel::Settings{.print_blob = false});
    Approvals::verify(out, scrub_report());
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

TEST(Diagnostics, QuietSuppressesEverything) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Quiet));
    Approvals::verify(out);
}

TEST(Diagnostics, NormalSuppressesNotesWhilePassing) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Normal));
    Approvals::verify(out);
}

// Verbose prints notes and drawn values on every case.
TEST(Diagnostics, VerbosePrintsNotesAndValues) {
    hegel::Settings settings = with_verbosity(hegel::Verbosity::Verbose);
    settings.seed = 1;
    settings.derandomize = false;
    std::string out = run_capturing_stderr(settings);
    Approvals::verify(out);
}

// Debug prints everything Verbose does (engine-side shrinker tracing is the
// engine's own output and not asserted on here).
TEST(Diagnostics, DebugPrintsNotesAndValues) {
    std::string out =
        run_capturing_stderr(with_verbosity(hegel::Verbosity::Debug));
    EXPECT_TRUE(contains(out, kNote));
    EXPECT_TRUE(contains(out, "auto draw_1 = "));
}

// A throw that isn't a std::exception exercises the catch(...) fallback,
// which records the exception's type name as the failure origin; the
// single-failure re-raise then surfaces the original exception, not a
// wrapper.
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

// ---------------------------------------------------------------------------
// Draw naming: bare names, 1-based suffixes for repeatable draws, one line
// per user-level draw. Deterministic outputs are snapshotted whole with
// ApprovalTests (tests/approvals/*.approved.txt).
// ---------------------------------------------------------------------------

namespace {
    // Runs one Verbose test case in-process and returns its stderr.
    std::string run_verbose(const std::function<void(hegel::TestCase&)>& body,
                            int64_t test_cases = 1) {
        testing::internal::CaptureStderr();
        hegel::test(body,
                    hegel::Settings{.test_cases = test_cases,
                                    .verbosity = hegel::Verbosity::Verbose,
                                    .database = hegel::Database::disabled()});
        return testing::internal::GetCapturedStderr();
    }
} // namespace

// A user-supplied name drawn once prints bare, with no suffix.
TEST(DrawNames, NamedDrawPrintsBareName) {
    std::string out = run_verbose(
        [](hegel::TestCase& tc) { (void)tc.draw("count", gs::just(5)); });
    Approvals::verify(out);
}

// Unnamed draws use the base name "draw" with 1-based suffixes, in draw
// order.
TEST(DrawNames, UnnamedDrawsAreNumbered) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw(gs::just(1));
        (void)tc.draw(gs::just(2));
    });
    Approvals::verify(out);
}

// A repeatable name is suffixed even for a single use.
TEST(DrawNames, RepeatableSingleUseIsSuffixed) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw("x", gs::just(3), /*repeatable=*/true);
    });
    Approvals::verify(out);
}

TEST(DrawNames, RepeatableDrawsNumberInOrder) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        for (int i = 1; i <= 3; ++i) {
            (void)tc.draw("x", gs::just(i), /*repeatable=*/true);
        }
    });
    Approvals::verify(out);
}

// Suffix allocation skips display names an earlier draw already took.
TEST(DrawNames, SuffixAllocationSkipsTakenNames) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw("x_1", gs::just(0));
        (void)tc.draw("x", gs::just(1), /*repeatable=*/true);
        (void)tc.draw("x", gs::just(2), /*repeatable=*/true);
    });
    Approvals::verify(out);
}

TEST(DrawNames, BareNameReusePrintsBare) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw("x", gs::just(1));
        (void)tc.draw("x", gs::just(2));
    });
    Approvals::verify(out);
}

TEST(DrawNames, MixedRepeatableAndBareUses) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw("x", gs::just(1), /*repeatable=*/true);
        (void)tc.draw("x", gs::just(2));
    });
    Approvals::verify(out);
}

// A collection draw prints one composed line, not one line per element.
TEST(DrawNames, CollectionDrawPrintsOneComposedLine) {
    std::string out = run_verbose([](hegel::TestCase& tc) {
        (void)tc.draw(gs::vectors(gs::just(5), {.min_size = 2, .max_size = 2}));
    });
    Approvals::verify(out);
}

// Draws inside a compose body are internal to the composed generator; only
// the outermost draw prints, with the final composed value.
TEST(DrawNames, ComposeInnerDrawsAreSilent) {
    auto gen = gs::compose([](const hegel::TestCase& tc) {
        int a = tc.draw(gs::just(1));
        int b = tc.draw(gs::just(2));
        return a + b;
    });
    std::string out =
        run_verbose([&gen](hegel::TestCase& tc) { (void)tc.draw(gen); });
    Approvals::verify(out);
}

TEST(DrawNames, CountersResetPerCase) {
    std::string out = run_verbose(
        [](hegel::TestCase& tc) {
            (void)tc.draw("x", gs::integers<int>().map([](int) { return 3; }),
                          /*repeatable=*/true);
        },
        /*test_cases=*/3);
    Approvals::verify(out);
}
