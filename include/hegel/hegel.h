#pragma once

/**
 * @mainpage Hegel
 *
 * Hegel is a property-based testing library for C++. Hegel is based on
 * [Hypothesis](https://github.com/hypothesisworks/hypothesis), using the
 * libhegel engine.
 *
 * @section getting_started Getting started
 *
 * This guide walks you through the basics of installing Hegel and writing
 * your first tests.
 *
 * @subsection install Install Hegel
 *
 * Using CMake:
 *
 * @code{.cmake}
 * include(FetchContent)
 * FetchContent_Declare(
 *     hegel
 *     GIT_REPOSITORY https://github.com/hegeldev/hegel-cpp.git
 *     GIT_TAG v0.10.0
 * )
 * FetchContent_MakeAvailable(hegel)
 *
 * target_link_libraries(your_target PRIVATE hegel)
 * @endcode
 *
 * Hegel requires CMake 3.14 and, by default, a C++20 compiler. The build
 * downloads a small prebuilt shared library (libhegel, Hegel's native engine)
 * for your platform; no other tooling is required.
 *
 * To consume Hegel from C++17, configure with `-DHEGEL_REFLECTION=OFF`. This
 * drops the reflect-cpp dependency: you lose @ref
 * hegel::generators::default_generator "default_generator" (type-directed
 * derivation for structs), but every other generator and combinator still
 * works. (The designated-initializer parameter API, e.g.
 * `integers<int>({.min_value = 0})`, then relies on a GCC/Clang C++17
 * extension.)
 *
 * @subsection first_test Write your first test
 *
 * You're now ready to write your first test. In a new file:
 *
 * @code{.cpp}
 * #include <hegel/hegel.h>
 *
 * namespace gs = hegel::generators;
 *
 * HEGEL_TEST(self_equality)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, n, gs::integers<int>());
 *     if (n != n) { // integers should always be equal to themselves
 *         throw std::runtime_error("self-equality failed");
 *     }
 * }
 *
 * int main() {
 *     return hegel::run_all_tests();
 * }
 * @endcode
 *
 * Now build and run the test. You should see that this test passes.
 *
 * Let's look at what's happening in more detail. @ref HEGEL_TEST defines a
 * property test as an ordinary function, `self_equality`, and registers it
 * with hegel::run_all_tests(), which runs every test defined this way in
 * the binary. You can also invoke the function individually — from `main()`
 * or from any test framework (a GoogleTest `TEST` body, for example).
 * Running the test executes your test body many times (100, by default).
 *
 * The body receives a @TestCase, which provides a @tc{draw}() method for
 * drawing different values. This test draws a random integer and checks
 * that it should be equal to itself. The macro also names the test in
 * Hegel's example database, so a failure found in one run is replayed first
 * in the next.
 *
 * @ref HEGEL_DRAW declares the variable and draws into in one step. You should
 * use this macro for drawing values. It records the variable's name so a
 * failing run replays each drawn value with the variable it was assigned to
 * (e.g. `auto n = 42`). The underlying @tc{draw}() call is still available
 * where a macro doesn't fit.
 *
 * Next, try a test that fails:
 *
 * @code{.cpp}
 * HEGEL_TEST(below_50)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, n, gs::integers<int>());
 *     if (n >= 50) { // this will fail!
 *         throw std::runtime_error("n should be below 50");
 *     }
 * }
 * @endcode
 *
 * This test asserts that any integer is less than 50, which is obviously
 * incorrect. Hegel will find a test case that makes this assertion fail,
 * and then shrink it to find the smallest counterexample. It reports:
 *
 * @code{.txt}
 * --- Failure: below_50 (my_test.cpp:3) ----------------------------------
 * Falsified after 3 test cases (0 discarded):
 *
 *   auto n = 50;
 *
 * Exception: std::runtime_error: n should be below 50
 * rerun with: HEGEL_REPRODUCE_FAILURE(below_50, "AAEAAAAACgEAAAAy")
 * @endcode
 *
 * The header names the test and where it is defined, the count says how
 * many cases it took to find the failure, and the falsifying value(s). The last
 * line replays that exact failure. See @ref HEGEL_REPRODUCE_FAILURE.
 *
 * To fix this test, you can constrain the integers you generate with the
 * `min_value` and `max_value` parameters:
 *
 * @code{.cpp}
 * HEGEL_TEST(below_50)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, n, gs::integers<int>({.min_value = 0, .max_value = 49}));
 *     if (n >= 50) {
 *         throw std::runtime_error("n should be below 50");
 *     }
 * }
 * @endcode
 *
 * Run the test again. It should now pass.
 *
 * @subsection use_generators Use generators
 *
 * Hegel provides a rich library of generators in the hegel::generators
 * namespace that you can use out of the box. There are primitive generators,
 * such as @integers, @floats, and @text, and combinators that allow you to
 * make generators out of other generators, such as @vectors and @tuples.
 *
 * For example, you can use @vectors to generate a vector of integers:
 *
 * @code{.cpp}
 * namespace gs = hegel::generators;
 *
 * HEGEL_TEST(push_back_grows)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, vector, gs::vectors(gs::integers<int>()));
 *     auto initial_length = vector.size();
 *     vector.push_back(tc.draw(gs::integers<int>()));
 *     if (vector.size() <= initial_length) {
 *         throw std::runtime_error("push_back should increase size");
 *     }
 * }
 * @endcode
 *
 * This test checks that appending an element to a random vector of integers
 * should always increase its length.
 *
 * You can also define custom generators. For example, say you have a
 * `Person` struct that we want to generate:
 *
 * @code{.cpp}
 * struct Person {
 *     int age;
 *     std::string name;
 * };
 *
 * auto generate_person() {
 *     return gs::compose([](const hegel::TestCase& tc) {
 *         int age = tc.draw(gs::integers<int>());
 *         std::string name = tc.draw(gs::text());
 *         return Person{age, name};
 *     });
 * }
 * @endcode
 *
 * Note that you can feed the results of a `draw` to subsequent calls. For
 * example, say that you extend the `Person` struct to include a
 * `driving_license` boolean field:
 *
 * @code{.cpp}
 * struct Person {
 *     int age;
 *     std::string name;
 *     bool driving_license;
 * };
 *
 * auto generate_person() {
 *     return gs::compose([](const hegel::TestCase& tc) {
 *         int age = tc.draw(gs::integers<int>());
 *         std::string name = tc.draw(gs::text());
 *         bool driving_license =
 *             age >= 18 ? tc.draw(gs::booleans()) : false;
 *         return Person{age, name, driving_license};
 *     });
 * }
 * @endcode
 *
 * Hegel can also derive generators automatically for reflectable structs
 * via @default_generator. This uses
 * [reflect-cpp](https://github.com/getml/reflect-cpp) to inspect the
 * struct's fields and pick an appropriate generator for each:
 *
 * @code{.cpp}
 * struct Person {
 *     std::string name;
 *     int age;
 * };
 *
 * HEGEL_TEST(generate_people)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, p, gs::default_generator<Person>());
 * }
 * @endcode
 *
 * Call `.override(...)` on the returned generator to customize individual
 * fields (see @ref hegel::generators::DerivedGenerator::override "override").
 *
 * @subsection checking_properties Check properties
 *
 * Hegel has three macros for stating what must hold.
 *
 * @ref HEGEL_REQUIRE_EQUAL states that two values are equal:
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, x, gs::integers<int>());
 *     HEGEL_DRAW(tc, y, gs::integers<int>());
 *     HEGEL_REQUIRE_EQUAL(tc, x + y, y + x);
 * }
 * @endcode
 *
 * The report shows a difference of the two values, down to the part that
 * changed, rather than only reporting that they differ.
 *
 * The two values must have the same type, and Hegel must be able to render
 * that type. A type of your own that is not an aggregate struct needs an
 * @c operator<< for a @c std::ostream, or the test does not compile. See
 * @ref HEGEL_REQUIRE_EQUAL for the types Hegel renders on its own.
 *
 * @ref HEGEL_REQUIRE states a condition, with a message the report prints:
 *
 * @code{.cpp}
 * HEGEL_TEST(running_sum_stays_positive)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, l, gs::vectors(gs::integers<int>()));
 *     HEGEL_REQUIRE(tc, lowest_running_sum(l) >= 0,
 *                   "running sum went negative");
 * }
 * @endcode
 *
 * @code{.txt}
 * --- Failure: running_sum_stays_positive (my_test.cpp:17) ---------------
 * Falsified after 2 test cases (0 discarded):
 *
 *   auto l = std::vector<int>{-1};
 *
 * Exception: running sum went negative
 * rerun with: HEGEL_REPRODUCE_FAILURE(running_sum_stays_positive,
 * "AAMAAAABAQAKAQAAAP8BAA==")
 * @endcode
 *
 * @ref HEGEL_FAIL fails outright, where there is no condition to write, such as
 * an unreachable branch, or a step that cannot go on:
 *
 * @code{.cpp}
 * HEGEL_TEST(parses_its_own_output)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, n, gs::integers<int>({.min_value = 0}));
 *     std::optional<int> parsed = parse(render(n));
 *     if (!parsed) {
 *         HEGEL_FAIL(tc, "rendered value did not parse back");
 *     }
 *     HEGEL_REQUIRE_EQUAL(tc, *parsed, n);
 * }
 * @endcode
 *
 * @code{.txt}
 * --- Failure: parses_its_own_output (my_test.cpp:23) --------------------
 * Falsified after 2 test cases (0 discarded):
 *
 *   auto n = 1000;
 *
 * Exception: rendered value did not parse back
 * rerun with: HEGEL_REPRODUCE_FAILURE(parses_its_own_output,
 * "AAEAAAAACgIAAADoAw==")
 * @endcode
 *
 * @ref HEGEL_FAIL does not return, so the branch above needs no value and
 * the code after it may use @c parsed.
 *
 * Prefer these macros over a raw @c throw. Hegel groups counterexamples by
 * origin to tell one bug from another, and a raw throw carries no source
 * position. Every throw of one type therefore shares an origin and reports as a
 * single bug. Each of invocation of these macros count as a different origin,
 * so failures on different lines stay distinct under
 * Settings::report_multiple_failures.
 *
 * @subsection debug_failing Debug your failing test cases
 *
 * Drawn values print automatically in the failure report (`auto x = ...;`).
 * Use @tc{note} to add whatever context the values alone do not show:
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, x, gs::integers<int>());
 *     HEGEL_DRAW(tc, y, gs::integers<int>());
 *     tc.note("x + y = " + std::to_string(x + y) +
 *             ", y + x = " + std::to_string(y + x));
 *     HEGEL_REQUIRE_EQUAL(tc, x + y, y + x);
 * }
 * @endcode
 *
 * Notes and drawn values print on the failing replay only. Raise
 * Settings::verbosity to Verbosity::Verbose to see them for every case.
 *
 * @subsection change_test_cases Change the number of test cases
 *
 * By default Hegel runs 100 test cases. To override this, write a
 * @Settings initializer after the test name:
 *
 * @code{.cpp}
 * HEGEL_TEST(self_equality, {.test_cases = 500})(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, n, gs::integers<int>());
 *     if (n != n) {
 *         throw std::runtime_error("self-equality failed");
 *     }
 * }
 * @endcode
 *
 * These settings are the test function's default argument; passing a
 * Settings when invoking the test (`self_equality({.test_cases = 5})`)
 * replaces them for that run.
 *
 * @subsection plain_function Use hegel::test() directly
 *
 * @ref HEGEL_TEST is a macro around hegel::test(), which remains
 * available when a macro doesn't fit — for example to run a lambda or
 * another callable built at runtime:
 *
 * @code{.cpp}
 * hegel::test([](hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, n, gs::integers<int>());
 *     if (n != n) {
 *         throw std::runtime_error("self-equality failed");
 *     }
 * }, {.test_cases = 500});
 * @endcode
 *
 * Unlike the macro, hegel::test() cannot derive a name for the test, so
 * set Settings::database_key yourself if you want failures persisted to
 * the example database and replayed across runs.
 *
 * @subsection learning_more Learning more
 *
 * - Browse the hegel::generators namespace for the full list of available
 *   generators.
 * - See @Settings for more configuration settings to customise how your
 *   test runs.
 */

#include "core.h"
#include "settings.h"
#include "test_case.h"

#include "generators/builds.h"
#include "generators/collections.h"
#include "generators/combinators.h"
#include "generators/default.h"
#include "generators/formats.h"
#include "generators/numeric.h"
#include "generators/primitives.h"
#include "generators/random.h"
#include "generators/stateful.h"
#include "generators/strings.h"

#include <functional>

/** @namespace hegel
 * @brief Main namespace
 */
namespace hegel {

    /**
     * @brief Where a test is defined.
     *
     * A failure report names the test and its source line in its header:
     * `--- Failure: my_property (tests/example.cpp:42) ---`. @ref HEGEL_TEST
     * fills this in from the test's name and position. A caller of
     * hegel::test() supplies it to get the same header.
     */
    struct TestLocation {
        /// Test name, as it appears in the report header.
        std::string name;
        /// Source file the test is defined in.
        std::string file;
        /// Line the test is defined on.
        int line = 0;
    };

    /**
     * @brief Run a Hegel test.
     *
     * This is the underlying entry point that @ref HEGEL_TEST (the
     * recommended way to define a test) expands to. Call it directly when a
     * macro doesn't fit, e.g. to run a lambda or another callable built at
     * runtime. In that case, set Settings::database_key yourself if you want
     * failures persisted to the example database and replayed across runs.
     *
     * @code{.cpp}
     * #include "hegel/hegel.h"
     *
     * int main() {
     *     hegel::test([](hegel::TestCase& tc) {
     *         namespace gs = hegel::generators;
     *         auto x = tc.draw(gs::integers<int>({.min_value = 0, .max_value =
     * 100})); auto y = tc.draw(gs::integers<int>({.min_value = 0, .max_value =
     * 100}));
     *
     *         // Property: x + y >= x (true for non-negative integers)
     *         if (x + y < x) {
     *             throw std::runtime_error("Addition underflow!");
     *         }
     *     }, {.test_cases = 1000});
     *
     *     return 0;
     * }
     * @endcode
     *
     * @param test_fn The test function to run repeatedly. Receives a
     *                TestCase which it uses to draw values, make
     *                assumptions, and record notes.
     * @param settings Configuration settings (test count, debug mode, etc.)
     * @param failure_blobs The base64 blobs encoding the engine choices that
     led to failures. Multiple blobs can be passed in for bookkeeping, but only
     the first one is run.
     * @throws std::runtime_error if any test case fails
     * @see Settings for configuration settings
     */
    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings = {},
              const std::vector<std::string>& failure_blobs = {});

    /**
     * @brief Run a Hegel test that names itself in its failure report.
     *
     * Behaves like the overload above. The failure report's header names
     * @p location's test and source line instead of reading `--- Failure ---`.
     *
     * @code{.cpp}
     * hegel::test(my_body, {"my_property", __FILE__, __LINE__},
     *             {.test_cases = 500});
     * @endcode
     *
     * @param test_fn The test function to run repeatedly.
     * @param location Where the test is defined.
     * @param settings Configuration settings (test count, debug mode, etc.)
     * @param failure_blobs The base64 blobs encoding the engine choices that
     led to failures. Multiple blobs can be passed in for bookkeeping, but only
     the first one is run.
     * @throws std::runtime_error if any test case fails
     */
    void test(const std::function<void(TestCase&)>& test_fn,
              const TestLocation& location, const Settings& settings = {},
              const std::vector<std::string>& failure_blobs = {});

    /**
     * @brief Run every test defined with @ref HEGEL_TEST in this binary.
     *
     * Each test runs with its inline settings.
     *
     * @code{.cpp}
     * int main() {
     *     return hegel::run_all_tests();
     * }
     * @endcode
     *
     * @return 0 if every test passed, 1 otherwise
     */
    int run_all_tests();

    /// @cond INTERNAL
    namespace internal {
        // Adds a HEGEL_TEST to the run_all_tests() registry during static
        // initialization; always returns true.
        // the only way the HEGEL_TEST macro can make something run before
        // main() is to hang the call off the initializer of a namespace-scope
        // variable
        bool register_test(const char* name, void (*run)());
        // add a blob for a test to run
        bool register_blob(const char* name, std::vector<const char*> blobs);
        // look up the failure blobs associated with a test
        std::vector<std::string> reproduce_blobs_for(const char* name);

        // What HEGEL_TEST expands to.
        void test_from_macro(const std::function<void(TestCase&)>& test_fn,
                             const TestLocation& location,
                             const Settings& settings,
                             const std::vector<std::string>& failure_blobs);
    } // namespace internal
    /// @endcond
} // namespace hegel

/**
 * @defgroup test_macros Test definition macros
 * @brief Macros for defining Hegel property tests.
 * @{
 */

/**
 * @brief Define a Hegel property test.
 *
 * It expands to a function `name(hegel::Settings = ...)` that runs that body
 * via hegel::test() with Settings::database_key defaulted to `"file.cpp::name"`
 * so counterexamples persisted to the database are scoped to this test and
 * replayed on later runs. A database_key set explicitly in the Settings
 * overrides the derived one.
 *
 * Settings for the test are written inline after the name. They become the
 * function's default argument, and settings passed at the call site replace
 * them entirely.
 *
 * The test is also registered with the test runtime, so all Hegel tests within
 * a translation unit can be run with hegel::run_all_tests().
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes, {.test_cases = 500})(hegel::TestCase& tc) {
 *     namespace gs = hegel::generators;
 *     int x = tc.draw(gs::integers<int>());
 *     int y = tc.draw(gs::integers<int>());
 *     if (x + y != y + x) {
 *         throw std::runtime_error("addition is not commutative");
 *     }
 * }
 *
 * // invoke from main() or any test framework:
 * addition_commutes();                  // runs with .test_cases = 500
 * addition_commutes({.test_cases = 5}); // call-site Settings take over
 * @endcode
 *
 * @see HEGEL_REPRODUCE_FAILURE to replay a specific failing example.
 */
#define HEGEL_TEST(name, ...)                                                  \
    static void hegel_test_body_##name(::hegel::TestCase&);                    \
    static void name(::hegel::Settings settings =                              \
                         ::hegel::Settings(__VA_ARGS__)) {                     \
        if (!settings.database_key.has_value()) {                              \
            settings.database_key = __FILE__ "::" #name;                       \
        }                                                                      \
        ::hegel::internal::test_from_macro(                                    \
            hegel_test_body_##name,                                            \
            ::hegel::TestLocation{#name, __FILE__, __LINE__}, settings,        \
            ::hegel::internal::reproduce_blobs_for(__FILE__ "::" #name));      \
    }                                                                          \
    [[maybe_unused]] static const bool hegel_test_registered_##name =          \
        ::hegel::internal::register_test(__FILE__ "::" #name, [] { name(); }); \
    static void hegel_test_body_##name

/**
 * @brief Draw a value into a named local variable.
 *
 * Expands to `auto var = tc.draw("var", gen)`, so failure output prints the
 * draw under the variable's own name:
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, x, gs::integers<int>());
 *     HEGEL_DRAW(tc, y, gs::integers<int>());
 *     if (x + y != y + x) throw std::runtime_error("not commutative");
 * }
 * // replay output: auto x = 10;
 * //                auto y = 3;
 * @endcode
 *
 * Prefer this macro over a plain @tc{draw}() for every draw bound to a
 * variable, since it includes the variable name in the counterexample printing.
 *
 * The name prints bare on every use. To number repeated draws instead
 * (`var_1`, `var_2`, ...), call `tc.draw("name", gen, true)`.
 *
 * @param tc The current hegel::TestCase.
 * @param var Name of the local variable to declare.
 * @param ... The generator expression to draw from.
 */
#define HEGEL_DRAW(tc, var, ...) auto var = (tc).draw(#var, (__VA_ARGS__))

/// @cond INTERNAL
// Stringification takes two steps because `#` reads its argument before any
// macro in it expands. `HEGEL_INTERNAL_STRINGIFY_(__LINE__)` alone therefore
// gives "__LINE__". Passing the argument through a macro that does not apply
// `#` expands it first, so the second macro stringifies the line number.
#define HEGEL_INTERNAL_STRINGIFY_(x) #x
#define HEGEL_INTERNAL_STRINGIFY(x) HEGEL_INTERNAL_STRINGIFY_(x)

// The call site, which the engine uses to tell one requirement from another.
// The three parts join into one string literal at compile time.
//
// `#` applies only to a parameter of a macro that takes parameters, so this
// macro cannot stringify __LINE__ itself. A bare `#__LINE__` here leaves the
// `#` in the token stream and fails to compile.
#define HEGEL_INTERNAL_ORIGIN __FILE__ ":" HEGEL_INTERNAL_STRINGIFY(__LINE__)
/// @endcond

/**
 * @brief Fail the current test case with @p message.
 *
 * @code{.cpp}
 * HEGEL_TEST(parser_accepts_its_own_output)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, doc, document_generator());
 *     auto reparsed = parse(render(doc));
 *     if (!reparsed) {
 *         HEGEL_FAIL(tc, "rendered document did not parse back");
 *     }
 * }
 * @endcode
 *
 * The macro does not return.
 *
 * Prefer this over a raw @c throw. Hegel groups counterexamples by origin to
 * tell one bug from another, and a raw throw carries no source position. Every
 * throw of one type therefore shares an origin and reports as a single bug.
 * Each @c HEGEL_FAIL counts as a different origin, so failures on different
 * lines stay distinct under Settings::report_multiple_failures.
 *
 * @param tc The current hegel::TestCase.
 * @param ... The optional failure message. Without one the report reads
 *            `fail: test case failed`.
 */
// The message is taken as trailing arguments rather than as one named
// parameter so that it may hold a comma the preprocessor would otherwise
// split on, as in a template argument list. __VA_ARGS__ puts those commas
// back. __VA_OPT__ drops the comma before it when no message is given, which
// is what makes the message optional.
#define HEGEL_FAIL(tc, ...)                                                    \
    ::hegel::internal::fail_impl((tc), HEGEL_INTERNAL_ORIGIN __VA_OPT__(, )    \
                                           __VA_ARGS__)

/**
 * @brief Fail the current test case unless @p condition holds.
 *
 * @code{.cpp}
 * HEGEL_TEST(sum_stays_positive)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, l, gs::vectors(gs::integers<int>({.min_value = 0})));
 *     HEGEL_REQUIRE(tc, running_sum(l) >= 0, "sum must stay non-negative");
 * }
 * @endcode
 *
 * The report prints the message in its `Exception:` line. Without one it
 * reads `require: condition was false`.
 *
 * @param tc The current hegel::TestCase.
 * @param ... The condition, and an optional message.
 */
#define HEGEL_REQUIRE(tc, ...)                                                 \
    ::hegel::internal::require_impl((tc), HEGEL_INTERNAL_ORIGIN, __VA_ARGS__)

/**
 * @brief Fail the current test case unless two values are equal.
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     HEGEL_DRAW(tc, x, gs::integers<int>());
 *     HEGEL_DRAW(tc, y, gs::integers<int>());
 *     HEGEL_REQUIRE_EQUAL(tc, x + y, y + x);
 * }
 * @endcode
 *
 * Prefer this over @ref HEGEL_REQUIRE for an equality property. The report
 * says how the two values differ.
 *
 * @code{.txt}
 * require_equal: values differ (- lhs / + rhs):
 *   Team{
 *     .name = std::string("a"),
 *     .scores = std::vector<int>{
 *       1,
 * -     2,
 * +     9,
 *       3,
 *     },
 *   }
 * @endcode
 *
 * The values are compared as Hegel renders them for the report.
 *
 * Hegel must also be able to render the type. A type it cannot render does
 * not compile:
 *
 * @code{.txt}
 * error: static assertion failed: HEGEL_REQUIRE_EQUAL needs a type Hegel can
 * render. Give it an operator<<, or compare its parts instead.
 * @endcode
 *
 * Hegel renders these types on its own:
 *
 * - the arithmetic types (@c bool, the character types, the integer types,
 *   the floating-point types) and enumerations
 * - @c std::string
 * - @c std::optional, @c std::pair, @c std::tuple, @c std::variant and
 *   @c std::monostate
 * - @c std::vector, @c std::set, @c std::map and @c std::array
 * - any nesting of the types above
 * - aggregate structs with no base class, through reflection. This needs a
 *   build with `HEGEL_REFLECTION` on, which is the default. Without
 *   reflection an aggregate struct is subject to the rule below.
 *
 * A type that is not in that list needs an @c operator<< that writes it to a
 * @c std::ostream. The rule applies to each part of a value. A
 * @c std::vector of a type Hegel cannot render does not compile either.
 *
 * @code{.cpp}
 * class Point {
 *   public:
 *     Point(int x, int y) : x_(x), y_(y) {}
 *
 *     friend std::ostream& operator<<(std::ostream& os, const Point& p) {
 *         return os << "Point(" << p.x_ << ", " << p.y_ << ")";
 *     }
 *
 *   private:
 *     int x_;
 *     int y_;
 * };
 * @endcode
 *
 * Where a type cannot get an @c operator<<, compare its parts instead.
 *
 * @param tc The current hegel::TestCase.
 * @param ... The two values to compare and an optional message.
 */
#define HEGEL_REQUIRE_EQUAL(tc, ...)                                           \
    ::hegel::internal::require_equal_impl((tc), HEGEL_INTERNAL_ORIGIN,         \
                                          __VA_ARGS__)

/**
 * @brief Replay a failing example for a @ref HEGEL_TEST from its blob.
 *
 * @code{.txt}
 * rerun with: HEGEL_REPRODUCE_FAILURE(addition_commutes, "AAEAAAAACgEAAAAA")
 * @endcode
 *
 * Place it above the matching HEGEL_TEST, keyed by the same name. The test
 * then replays that example instead of generating new cases. Delete the
 * annotation to return to a normal run. Settings::print_blob controls whether
 * the report prints the line.
 *
 * At least one blob is required. Additional blobs are accepted for bookkeeping,
 * but only the first is replayed.
 *
 * @code{.cpp}
 * HEGEL_REPRODUCE_FAILURE(addition_commutes, "AAEAAAAACgEAAAAA")
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     // ...
 * }
 * @endcode
 *
 * @see HEGEL_TEST
 */
#define HEGEL_REPRODUCE_FAILURE(name, blob, ...)                               \
    [[maybe_unused]] static const bool hegel_reproduce_registered_##name =     \
        ::hegel::internal::register_blob(__FILE__ "::" #name,                  \
                                         {blob, __VA_ARGS__});
/** @} */
