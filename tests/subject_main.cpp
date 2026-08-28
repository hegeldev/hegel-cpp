// Prebuilt helper binary for the Output tests. Each scenario runs a property
// that fails in a specific way; main() prints the surfaced exception message
// to stderr and exits non-zero, which the Output tests inspect. Selecting
// the scenario by argv avoids recompiling per test.
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

namespace {
    struct MyError {};

    hegel::Settings no_database() {
        return {.database = hegel::Database::disabled()};
    }

    // Fails on every case; the minimal counterexample is 0.
    void scenario_failing() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                throw std::runtime_error("intentional failure: " +
                                         std::to_string(x));
            },
            no_database());
    }

    // Fails for x >= 10; shrinks to 10.
    void scenario_stable_origin() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 10) {
                    throw std::runtime_error("failure with x=" +
                                             std::to_string(x));
                }
            },
            no_database());
    }

    // Two `throw`s of one type, so two failure origins. Run in a fresh
    // process, this shows a throw site staying stable under ASLR: the origin
    // holds an offset from the enclosing function, never a load address.
    void scenario_throw_sites() {
        hegel::test(
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
            hegel::Settings{.report_multiple_failures = true,
                            .database = hegel::Database::disabled()});
    }

    // Throws a non-std exception (int) for x >= 5.
    void scenario_throw_int() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 5) {
                    throw 42;
                }
            },
            no_database());
    }

    // Throws a custom non-std exception type for x >= 5.
    void scenario_throw_custom() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 5) {
                    throw MyError{};
                }
            },
            no_database());
    }

    // Two distinct bugs (different exception types, so different origins):
    // even x >= 10 shrinks to 10, odd x >= 10 shrinks to 11.
    void scenario_multiple_failures() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 10 && x % 2 == 0) {
                    throw std::runtime_error("even bug with x=" +
                                             std::to_string(x));
                }
                if (x >= 10 && x % 2 != 0) {
                    throw std::logic_error("odd bug with x=" +
                                           std::to_string(x));
                }
            },
            hegel::Settings{.report_multiple_failures = true,
                            .database = hegel::Database::disabled()});
    }

    // Same two bugs, but report_multiple_failures = false: the run stops at
    // the first failing example and reports a single failure.
    void scenario_multiple_failures_off() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 10 && x % 2 == 0) {
                    throw std::runtime_error("even bug with x=" +
                                             std::to_string(x));
                }
                if (x >= 10 && x % 2 != 0) {
                    throw std::logic_error("odd bug with x=" +
                                           std::to_string(x));
                }
            },
            hegel::Settings{.report_multiple_failures = false,
                            .database = hegel::Database::disabled()});
    }

    // Surfaces the thrown exception's message; shrinks to x=7.
    void scenario_exception_message() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 7) {
                    throw std::runtime_error("custom exception for x=" +
                                             std::to_string(x));
                }
            },
            no_database());
    }

    void scenario_print_blob() {
        hegel::test(
            [](hegel::TestCase& tc) {
                int32_t x = tc.draw(gs::integers<int32_t>());
                if (x >= 0) {
                    throw std::runtime_error("silly error");
                }
            },
            {.print_blob = true, .database = hegel::Database::disabled()});
    }
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: subject <scenario>\n");
        return 2;
    }
    const std::string scenario = argv[1];
    try {
        if (scenario == "failing") {
            scenario_failing();
        } else if (scenario == "stable_origin") {
            scenario_stable_origin();
        } else if (scenario == "throw_sites") {
            scenario_throw_sites();
        } else if (scenario == "throw_int") {
            scenario_throw_int();
        } else if (scenario == "throw_custom") {
            scenario_throw_custom();
        } else if (scenario == "multiple_failures") {
            scenario_multiple_failures();
        } else if (scenario == "multiple_failures_off") {
            scenario_multiple_failures_off();
        } else if (scenario == "exception_message") {
            scenario_exception_message();
        } else if (scenario == "print_blob") {
            scenario_print_blob();
        } else {
            std::fprintf(stderr, "unknown scenario: %s\n", argv[1]);
            return 2;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "test failed with a non-std exception\n");
        return 1;
    }
    return 0;
}
