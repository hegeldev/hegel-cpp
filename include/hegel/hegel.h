#pragma once

/**
 * @mainpage Hegel C++ SDK - Hypothesis-like property-based testing for C++20
 *
 * This library provides type-safe random data generation for property-based
 * testing, communicating with the Hegel test runner via Unix sockets.
 *
 * @section usage Basic Usage
 *
 * @code{.cpp}
 * #include "hegel/hegel.h"
 *
 * hegel::hegel([]() {
 *     using namespace hegel::generators;
 *
 *     // Type-based generation (schema derived via reflect-cpp)
 *     Person p = hegel::draw(hegel::generators::default_generator<Person>());
 *
 *     // Generator-based generation
 *     int value = hegel::draw(integers<int>({.min_value = 0, .max_value =
 * 100}));
 * });
 * @endcode
 *
 * @section deps Dependencies
 * - reflect-cpp (https://github.com/getml/reflect-cpp) - C++20 reflection
 * - nlohmann library - CBOR serialization
 * - POSIX sockets (sys/socket.h, sys/un.h, unistd.h)
 */

// HegelOptions and supporting classes
#include "options.h"

// Core generator types
#include "core.h"

// Generator functions by category
#include "generators/builds.h"
#include "generators/collections.h"
#include "generators/combinators.h"
#include "generators/formats.h"
#include "generators/numeric.h"
#include "generators/primitives.h"
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
     *     using namespace hegel::generators;
     *     auto x = hegel::draw(integers<int>({.min_value = 0, .max_value =
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
     * 1. Creates a Unix socket server for communication
     * 2. Spawns the Hegel CLI as a subprocess
     * 3. Runs the test function for each generated test case
     * 4. Handles shrinking when failures occur
     * 5. Throws std::runtime_error if any test case fails
     *
     * @code{.cpp}
     * #include "hegel/hegel.h"
     *
     * int main() {
     *     hegel::hegel([]() {
     *         using namespace hegel::generators;
     *         auto x = hegel::draw(integers<int>({.min_value = 0, .max_value =
     * 100})); auto y = hegel::draw(integers<int>({.min_value = 0, .max_value =
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
     * @tparam F Test function type (callable with no arguments)
     * @param test_fn The test function to run repeatedly
     * @param options Configuration options (test count, debug mode, hegel path)
     * @throws std::runtime_error if any test case fails
     * @see HegelOptions for configuration options
     */
    void hegel(std::function<void()> test_fn,
               options::HegelOptions options = {});
} // namespace hegel
