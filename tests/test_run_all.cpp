// Standalone check for hegel::run_all_tests(): every HEGEL_TEST below must
// run (the failures must not stop the sweep), and the return value must
// report the failures. A test body that throws a value that is not a
// std::exception (here: an int) must be counted as a failure like any other,
// not escape and kill the process. Exits 0 on success.

#include <cstdio>
#include <hegel/hegel.h>
#include <stdexcept>

namespace gs = hegel::generators;

static int passing_runs = 0;

HEGEL_TEST(always_fails,
           {.database = hegel::Database::disabled()})(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    throw std::runtime_error("always fails");
}

HEGEL_TEST(throws_non_std_exception,
           {.database = hegel::Database::disabled()})(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    throw 42; // not derived from std::exception
}

HEGEL_TEST(always_passes,
           {.test_cases = 5, .database = hegel::Database::disabled()})
(hegel::TestCase& tc) {
    tc.draw(gs::integers<int>());
    passing_runs++;
}

int main() {
    int rc = hegel::run_all_tests();
    if (rc != 1) {
        std::fprintf(stderr, "expected run_all_tests() to return 1, got %d\n",
                     rc);
        return 1;
    }
    if (passing_runs != 5) {
        std::fprintf(stderr, "expected the passing test to run 5 cases, %d\n",
                     passing_runs);
        return 1;
    }
    std::puts("run_all_tests OK");
    return 0;
}
