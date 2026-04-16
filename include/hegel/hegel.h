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
 * hegel::hegel([](hegel::TestCase& tc) {
 *     namespace gs = hegel::generators;
 *
 *     // Type-based generation (schema derived via reflect-cpp)
 *     Person p = tc.draw(gs::default_generator<Person>());
 *
 *     // Generator-based generation
 *     int value = tc.draw(gs::integers<int>({.min_value = 0, .max_value =
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

// TestCase passed to the test callback
#include "test_case.h"

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

    /**
     * @brief Run property-based tests using Hegel in embedded mode.
     *
     * This is the recommended way to run property tests. The function:
     * 1. Spawns the Hegel CLI as a subprocess, connected via pipes to its
     *    stdin and stdout
     * 2. Runs the test function for each generated test case, passing it a
     *    fresh TestCase handle
     * 3. Handles shrinking when failures occur
     * 4. Throws std::runtime_error if any test case fails
     *
     * @code{.cpp}
     * #include "hegel/hegel.h"
     *
     * int main() {
     *     hegel::hegel([](hegel::TestCase& tc) {
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
     *                TestCase& which it uses to draw values, make
     *                assumptions, and record notes.
     * @param settings Configuration settings (test count, debug mode, etc.)
     * @throws std::runtime_error if any test case fails
     * @see HegelSettings for configuration settings
     */
    void hegel(const std::function<void(TestCase&)>& test_fn,
               const settings::HegelSettings& settings = {});
} // namespace hegel
