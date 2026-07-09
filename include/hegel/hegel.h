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
 *     GIT_TAG v0.7.0
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
 *     int n = tc.draw(gs::integers<int>());
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
 * Next, try a test that fails:
 *
 * @code{.cpp}
 * HEGEL_TEST(below_50)(hegel::TestCase& tc) {
 *     int n = tc.draw(gs::integers<int>());
 *     if (n >= 50) { // this will fail!
 *         throw std::runtime_error("n should be below 50");
 *     }
 * }
 * @endcode
 *
 * This test asserts that any integer is less than 50, which is obviously
 * incorrect. Hegel will find a test case that makes this assertion fail,
 * and then shrink it to find the smallest counterexample — in this case,
 * `n = 50`.
 *
 * To fix this test, you can constrain the integers you generate with the
 * `min_value` and `max_value` parameters:
 *
 * @code{.cpp}
 * HEGEL_TEST(below_50)(hegel::TestCase& tc) {
 *     int n = tc.draw(gs::integers<int>({.min_value = 0, .max_value = 49}));
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
 *     auto vector = tc.draw(gs::vectors(gs::integers<int>()));
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
 *     Person p = tc.draw(gs::default_generator<Person>());
 * }
 * @endcode
 *
 * Call `.override(...)` on the returned generator to customize individual
 * fields (see @ref hegel::generators::DerivedGenerator::override "override").
 *
 * @subsection debug_failing Debug your failing test cases
 *
 * Use @tc{note} to attach debug information:
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     int x = tc.draw(gs::integers<int>());
 *     int y = tc.draw(gs::integers<int>());
 *     tc.note("x + y = " + std::to_string(x + y) +
 *             ", y + x = " + std::to_string(y + x));
 *     if (x + y != y + x) {
 *         throw std::runtime_error("addition is not commutative");
 *     }
 * }
 * @endcode
 *
 * Notes only appear when Hegel replays the minimal failing example.
 *
 * @subsection change_test_cases Change the number of test cases
 *
 * By default Hegel runs 100 test cases. To override this, write a
 * @Settings initializer after the test name:
 *
 * @code{.cpp}
 * HEGEL_TEST(self_equality, {.test_cases = 500})(hegel::TestCase& tc) {
 *     int n = tc.draw(gs::integers<int>());
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
 *     int n = tc.draw(gs::integers<int>());
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
#include "generators/strings.h"

#include <functional>

/** @namespace hegel
 * @brief Main namespace
 */
namespace hegel {

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
     *                      led to failures. When non-empty, generation is
     *                      skipped and **only the first blob is replayed** —
     *                      this matches the other Hegel implementations, where
     *                      stacked reproduce-failure annotations are
     *                      first-wins. Any further blobs are carried for
     *                      bookkeeping only and are never run.
     * @throws The test body's own exception when a single failing example is
     *         found: after shrinking, the failure is replayed and the
     *         exception the body threw on the minimal example is rethrown
     *         as-is.
     * @throws std::runtime_error if the run itself fails (engine error,
     *                            health-check failure, flaky test), if a
     *                            failure blob does not reproduce an error, or
     *                            if multiple distinct failures are reported
     *                            (Settings::report_multiple_failures)
     * @see Settings for configuration settings
     */
    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings = {},
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
        ::hegel::test(                                                         \
            hegel_test_body_##name, settings,                                  \
            ::hegel::internal::reproduce_blobs_for(__FILE__ "::" #name));      \
    }                                                                          \
    [[maybe_unused]] static const bool hegel_test_registered_##name =          \
        ::hegel::internal::register_test(__FILE__ "::" #name, [] { name(); }); \
    static void hegel_test_body_##name

/**
 * @brief Replay a failing example for a @ref HEGEL_TEST from its blob.
 *
 * Place this above the matching HEGEL_TEST, keyed by the same name. The test
 * then replays the given reproduction blob (as printed by Settings::print_blob)
 * instead of generating new cases. Delete the annotation to return to a normal
 * run.
 *
 * At least one blob is required. Additional blobs may be listed to keep track
 * of several failures, but **only the first is replayed** — the rest are
 * bookkeeping, to be deleted one by one as the failures are fixed. This
 * first-wins rule matches the other Hegel implementations.
 *
 * Each test may have at most one HEGEL_REPRODUCE_FAILURE annotation.
 * Registering a second one for the same test is an error: the program
 * terminates during static initialization with a message naming the test.
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
