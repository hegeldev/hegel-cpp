#pragma once

/**
 * @mainpage Hegel
 *
 * Hegel is a property-based testing library for C++. Hegel is based on
 * [Hypothesis](https://github.com/hypothesisworks/hypothesis), using the
 * [Hegel](https://hegel.dev/) protocol.
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
 *     GIT_TAG v0.2.9
 * )
 * FetchContent_MakeAvailable(hegel)
 *
 * target_link_libraries(your_target PRIVATE hegel)
 * @endcode
 *
 * Hegel requires C++20, CMake 3.14, and [`uv`](https://docs.astral.sh/uv/) on
 * the PATH.
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
 * int main() {
 *     hegel::test([](hegel::TestCase& tc) {
 *         int n = tc.draw(gs::integers<int>());
 *         if (n != n) { // integers should always be equal to themselves
 *             throw std::runtime_error("self-equality failed");
 *         }
 *     });
 *     return 0;
 * }
 * @endcode
 *
 * Now build and run the test. You should see that this test passes.
 *
 * Let's look at what's happening in more detail. hegel::test() runs your
 * test callback many times (100, by default). The callback receives a
 * @TestCase, which provides a
 * @tc{draw}() method for drawing different
 * values. This test draws a random integer and checks that it should be
 * equal to itself.
 *
 * Next, try a test that fails:
 *
 * @code{.cpp}
 * hegel::test([](hegel::TestCase& tc) {
 *     int n = tc.draw(gs::integers<int>());
 *     if (n >= 50) { // this will fail!
 *         throw std::runtime_error("n should be below 50");
 *     }
 * });
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
 * hegel::test([](hegel::TestCase& tc) {
 *     int n = tc.draw(gs::integers<int>({.min_value = 0, .max_value = 49}));
 *     if (n >= 50) {
 *         throw std::runtime_error("n should be below 50");
 *     }
 * });
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
 * hegel::test([](hegel::TestCase& tc) {
 *     auto vector = tc.draw(gs::vectors(gs::integers<int>()));
 *     auto initial_length = vector.size();
 *     vector.push_back(tc.draw(gs::integers<int>()));
 *     if (vector.size() <= initial_length) {
 *         throw std::runtime_error("push_back should increase size");
 *     }
 * });
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
 * hegel::test([](hegel::TestCase& tc) {
 *     Person p = tc.draw(gs::default_generator<Person>());
 * });
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
 * hegel::test([](hegel::TestCase& tc) {
 *     int x = tc.draw(gs::integers<int>());
 *     int y = tc.draw(gs::integers<int>());
 *     tc.note("x + y = " + std::to_string(x + y) +
 *             ", y + x = " + std::to_string(y + x));
 *     if (x + y != y + x) {
 *         throw std::runtime_error("addition is not commutative");
 *     }
 * });
 * @endcode
 *
 * Notes only appear when Hegel replays the minimal failing example.
 *
 * @subsection change_test_cases Change the number of test cases
 *
 * By default Hegel runs 100 test cases. To override this, pass a
 * @Settings struct as the second argument to hegel::test():
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
     * @code{.cpp}
     * #include "hegel/hegel.h"
     *
     * int main() {
     *     hegel::test([](hegel::TestCase& tc) {
     *         namespace gs = hegel::generators;
     *         auto x = tc.draw(gs::integers<int>({.min_value = 0,
     * .max_value = 100})); auto y = tc.draw(gs::integers<int>({.min_value =
     * 0, .max_value = 100}));
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
     * @throws std::runtime_error if any test case fails
     * @see Settings for configuration settings
     */
    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings = {});
} // namespace hegel
