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
 *     GIT_TAG v0.12.0
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
 *     auto n = tc.draw("n", gs::integers<int>());
 *     if (n != n) { // integers should always be equal to themselves
 *         throw std::runtime_error("self-equality failed");
 *     }
 * }
 *
 * int main() {
 *     self_equality();
 *     return 0;
 * }
 * @endcode
 *
 * Now build and run the test. You should see that this test passes.
 *
 * Let's look at what's happening in more detail. @ref HEGEL_TEST defines a
 * property test as an ordinary function, `self_equality`. Running the test
 * executes your test body 100 times by default. While you may call the
 * function from `main()`, if you use a test framework, write the property
 * inside one of its tests instead. We officially support gtest (see @ref
 * use_gtest). Please make an issue on Github if Hegel does not integrate well
 * with your framework.
 *
 * The body receives a @TestCase, which provides a @tc{draw}() method for
 * drawing different values. This test draws a random integer and checks
 * that it should be equal to itself. The macro also names the test in
 * Hegel's example database, so a failure found in one run is replayed first
 * in the next.
 *
 * The name you pass to @tc{draw}() labels the value in the failure report, so
 * a failing run replays each drawn value under the variable it was assigned to
 * (e.g. `auto n = 42`). Give each draw the name of its variable.
 * @tc{draw}(gen) without a name prints numbered placeholders
 * (`auto draw_1 = ...;`) instead.
 *
 * Next, try a test that fails:
 *
 * @code{.cpp}
 * HEGEL_TEST(below_50)(hegel::TestCase& tc) {
 *     auto n = tc.draw("n", gs::integers<int>());
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
 *     auto n = tc.draw(
 *         "n", gs::integers<int>({.min_value = 0, .max_value = 49}));
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
 *     auto vector = tc.draw("vector", gs::vectors(gs::integers<int>()));
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
 *     auto p = tc.draw("p", gs::default_generator<Person>());
 * }
 * @endcode
 *
 * Call `.override(...)` on the returned generator to customize individual
 * fields (see @ref hegel::generators::DerivedGenerator::override "override").
 *
 * @subsection checking_properties Check properties
 *
 * A test body states what must hold by throwing when it does not. Any
 * exception fails the test case, and Hegel then shrinks the values that
 * caused it:
 *
 * @code{.cpp}
 * HEGEL_TEST(running_sum_stays_positive)(hegel::TestCase& tc) {
 *     auto l = tc.draw("l", gs::vectors(gs::integers<int>()));
 *     if (lowest_running_sum(l) < 0) {
 *         throw std::runtime_error("running sum went negative");
 *     }
 * }
 * @endcode
 *
 * @code{.txt}
 * --- Failure: running_sum_stays_positive (my_test.cpp:17) ---------------
 * Falsified after 2 test cases (0 discarded):
 *
 *   auto l = std::vector<int>{-1};
 *
 * Exception: std::runtime_error: running sum went negative
 * rerun with: HEGEL_REPRODUCE_FAILURE(running_sum_stays_positive,
 * "AAMAAAABAQAKAQAAAP8BAA==")
 * @endcode
 *
 * The assertion macros of a test framework work too. See @ref use_gtest for
 * GoogleTest.
 *
 * Hegel groups counterexamples by origin to tell one bug from another. The
 * origin of a thrown exception is its type and the site it was thrown from.
 * Derive the exception from @ref hegel::FailureOrigin "FailureOrigin" to group
 * them some other way, but the origin must be stable.
 *
 * @subsection debug_failing Debug your failing test cases
 *
 * Drawn values print automatically in the failure report (`auto x = ...;`).
 * Use @tc{note} to add whatever context the values alone do not show:
 *
 * @code{.cpp}
 * HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
 *     auto x = tc.draw("x", gs::integers<int>());
 *     auto y = tc.draw("y", gs::integers<int>());
 *     tc.note("x + y = " + std::to_string(x + y) +
 *             ", y + x = " + std::to_string(y + x));
 *     if (x + y != y + x) {
 *         throw std::runtime_error("addition is not commutative");
 *     }
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
 *     auto n = tc.draw("n", gs::integers<int>());
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
 * @subsection use_gtest Hegel and test frameworks
 *
 * With a test framework, write the property inside one of its tests and call
 * hegel::test() with the body. You cannot use @ref HEGEL_TEST there.
 *
 * @code{.cpp}
 * #include <gtest/gtest.h>
 * #include <hegel/hegel.h>
 *
 * TEST(Arithmetic, AdditionCommutes) {
 *     hegel::test([](hegel::TestCase& tc) {
 *         auto x = tc.draw("x", gs::integers<int>());
 *         auto y = tc.draw("y", gs::integers<int>());
 *         ASSERT_EQ(x + y, y + x);
 *     });
 * }
 * @endcode
 *
 * @code{.txt}
 * --- Failure: Arithmetic.AdditionCommutes (arithmetic_test.cpp:8) ---
 * Falsified after 2 test cases (0 discarded):
 *
 *   auto x = 51;
 *   auto y = 0;
 *
 * Exception: hegel::GTestFailure: arithmetic_test.cpp:11: Expected: (x + y) ==
 * (y + x), actual: 51 vs 0
 * @endcode
 *
 * Outside a test framework, use @ref HEGEL_TEST, since it derives the database
 * key and test location for you. hegel::test() can still be used, but you will
 * have to set Settings::database_key yourself if you want failures persisted to
 * the example database. You will also have to pass in TestLocation to see test
 * location information in the failure output.
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

#if defined(__GNUC__) || defined(__clang__) ||                                 \
    (defined(_MSC_VER) && _MSC_VER >= 1927)
#define HEGEL_CALLER_FILE __builtin_FILE()
#define HEGEL_CALLER_FUNCTION __builtin_FUNCTION()
#define HEGEL_CALLER_LINE __builtin_LINE()
#else
#define HEGEL_CALLER_FILE ""
#define HEGEL_CALLER_FUNCTION ""
#define HEGEL_CALLER_LINE 0
#endif

/** @namespace hegel
 * @brief Main namespace
 */
namespace hegel {

    /**
     * @brief Interface for an exception that names its own failure origin.
     *
     * Hegel groups counterexamples by origin to tell one bug from another.
     * By default, the origin of a thrown exception is its type and the site it
     * was thrown from.
     *
     * Derive from this interface to define origins some other way. The origin
     * must be stable.
     *
     * @code{.cpp}
     * class CheckFailed : public std::runtime_error,
     *                     public hegel::FailureOrigin {
     *   public:
     *     CheckFailed(const char* file, int line, const std::string& message)
     *         : std::runtime_error(message),
     *           origin_(std::string(file) + ":" + std::to_string(line)) {}
     *
     *     std::string failure_origin() const override { return origin_; }
     *
     *   private:
     *     std::string origin_;
     * };
     * @endcode
     *
     */
    class FailureOrigin {
      public:
        FailureOrigin() = default;
        virtual ~FailureOrigin() = default;
        /// Copy-constructs. An origin holds no state of its own.
        FailureOrigin(const FailureOrigin&) = default;
        /// Copy-assigns. An origin holds no state of its own.
        /// @return Reference to this interface.
        FailureOrigin& operator=(const FailureOrigin&) = default;

        /**
         * @brief The origin grouping this failure.
         * @return A string naming the bug this exception stands for.
         */
        virtual std::string failure_origin() const = 0;
    };

    /**
     * @brief Where a test is defined.
     *
     * A failure report names the test and its source line in its header:
     * `--- Failure: my_property (tests/example.cpp:42) ---`. @ref HEGEL_TEST
     * fills this in from the test's name and position. A property inside
     * a GoogleTest test fills this in from that test's name and the line of the
     * hegel::test() call.
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
     * This is the underlying entry point that @ref HEGEL_TEST expands to.
     *
     * Settings::database_key defaults to `"<file>::<name>"`, where the file
     * is the one holding the call and the name is the enclosing GoogleTest
     * test (`Suite.Name`) or, outside a test framework, the enclosing
     * function.
     *
     * Inside a GoogleTest test the failure report's header names that test
     * and the line of the call. Outside one it reads `--- Failure ---`. Pass
     * a TestLocation to the overload below to name it yourself.
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
     * @param caller_file Filled in by the compiler with the file of the call.
     * @param caller_function Filled in by the compiler with the function
     *                        holding the call.
     * @param caller_line Filled in by the compiler with the line of the call.
     * @throws std::runtime_error if any test case fails
     * @see Settings for configuration settings
     */
    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings = {},
              const std::vector<std::string>& failure_blobs = {},
              const char* caller_file = HEGEL_CALLER_FILE,
              const char* caller_function = HEGEL_CALLER_FUNCTION,
              int caller_line = HEGEL_CALLER_LINE);

    /**
     * @brief Run a Hegel test that names itself in its failure report.
     *
     * Behaves like the overload above. The failure report's header names
     * @p location's test and source line instead of reading `--- Failure ---`,
     * and Settings::database_key defaults to the location's file and name.
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

    /// @cond INTERNAL
    namespace internal {
        // How a test framework passes information to Hegel
        struct FrameworkHooks {
            // Names the framework test that runs now, or "" outside one.
            std::string (*current_test_name)() = nullptr;
            // Runs one test-case body, raising the failed assertions the
            // framework recorded.
            void (*run_case)(const std::function<void()>&) = nullptr;
        };

        // Makes `hooks` the integration every later run uses. Returns true, so
        // an integration header can install from a variable initializer and
        // have it happen before main().
        bool install_framework_hooks(const FrameworkHooks& hooks);

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
 * With a test framework, write the property inside one of its tests and call
 * hegel::test() instead. If you use HEGEL_TEST, it must be used outside of
 * the test framework's test macro/function then call the named Hegel test.
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
 * int main() {
 *     addition_commutes();                  // runs with .test_cases = 500
 *     addition_commutes({.test_cases = 5}); // call-site Settings take over
 *     return 0;
 * }
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
    static void hegel_test_body_##name

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

// GoogleTest integration, picked up when <gtest/gtest.h> is already visible.
// Include <hegel/gtest.h> yourself if this header comes first.
#if defined(GTEST_TEST)
#include "gtest.h"
#endif
