#pragma once

/**
 * @mainpage Hegel - Hypothesis-like property-based testing for C++20
 *
 * This library provides type-safe random data generation for property-based
 * testing, spawning the Hegel test runner as a subprocess and communicating
 * with it over its stdin/stdout.
 *
 * @section usage Basic Usage
 *
 * @code{.cpp}
 * #include "hegel/hegel.h"
 *
 * struct Person {
 *     std::string name;
 *     int age;
 * };
 *
 * hegel::hegel([]() {
 *     namespace gs = hegel::generators;
 *
 *     // Type-based generation (schema derived via reflect-cpp)
 *     Person p = hegel::draw(gs::default_generator<Person>());
 *
 *     // Generator-based generation
 *     int value = hegel::draw(gs::integers<int>({.min_value = 0, .max_value =
 * 100}));
 * });
 * @endcode
 *
 * @section deps Dependencies
 * - reflect-cpp (https://github.com/getml/reflect-cpp) - C++20 reflection
 * - nlohmann library
 */

// HegelSettings and supporting classes
#include "settings.h"

// Core generator types
#include "core.h"

// Generator functions by category
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
 * @brief Main Hegel namespace
 *
 * All Hegel functionality exists in this namespace or its sub-namespaces.
 */
namespace hegel {

    // Public API re-exports from hegel::internal
    using internal::assume;
    using internal::note;
    using internal::stop_test;

    /**
     * @brief Draw a random value from a generator.
     *
     * This is the primary way to get values from generators inside a
     * Hegel test. Throws if called outside of a hegel() test.
     *
     * @code{.cpp}
     * hegel::hegel([]() {
     *     namespace gs = hegel::generators;
     *     auto x = hegel::draw(gs::integers<int>({.min_value = 0, .max_value =
     * 100}));
     * });
     * @endcode
     *
     * @tparam T The generated value type
     * @param gen The generator to draw from
     * @return A randomly generated value of type T
     * @throws std::runtime_error if called outside of a Hegel test
     */
    template <typename T> T draw(const generators::Generator<T>& gen) {
        auto* data = internal::get_test_case_data();
        if (!data) {
            throw std::runtime_error(
                "draw() cannot be called outside of a Hegel test");
        }
        return gen.do_draw(data);
    }

    /**
     * @brief Run property-based tests using Hegel in embedded mode.
     *
     * This is the recommended way to run property tests. The function:
     * 1. Spawns the Hegel CLI as a subprocess, connected via pipes to its
     *    stdin and stdout
     * 2. Runs the test function for each generated test case
     * 3. Handles shrinking when failures occur
     * 4. Throws std::runtime_error if any test case fails
     *
     * @code{.cpp}
     * #include "hegel/hegel.h"
     *
     * int main() {
     *     hegel::hegel([]() {
     *         namespace gs = hegel::generators;
     *         auto x = hegel::draw(gs::integers<int>({.min_value = 0,
     * .max_value = 100})); auto y = hegel::draw(gs::integers<int>({.min_value =
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
     * @tparam F Test function type (callable with no arguments)
     * @param test_fn The test function to run repeatedly
     * @param settings Configuration settings (test count, debug mode, etc.)
     * @throws std::runtime_error if any test case fails
     * @see HegelSettings for configuration settings
     */
    void hegel(const std::function<void()>& test_fn,
               const settings::HegelSettings& settings = {});
} // namespace hegel
