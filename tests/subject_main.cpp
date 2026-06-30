// Prebuilt helper binary for the Output tests. Each scenario runs a property
// that fails in a specific way; the uncaught exception from hegel::test() makes
// the process exit non-zero and print the failure to stderr, which the Output
// tests inspect. Selecting the scenario by argv avoids recompiling per test.
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
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: subject <scenario>\n");
        return 2;
    }
    const std::string scenario = argv[1];
    if (scenario == "failing") {
        scenario_failing();
    } else if (scenario == "stable_origin") {
        scenario_stable_origin();
    } else if (scenario == "throw_int") {
        scenario_throw_int();
    } else if (scenario == "throw_custom") {
        scenario_throw_custom();
    } else if (scenario == "exception_message") {
        scenario_exception_message();
    } else {
        std::fprintf(stderr, "unknown scenario: %s\n", argv[1]);
        return 2;
    }
    return 0;
}
