#pragma once

/**
 * @file testing.h
 * @brief Test utilities for Hegel property-based testing.
 *
 * Provides helper functions for testing generators and properties:
 * - assert_all_examples: verify a predicate holds for all generated values
 * - assert_no_examples: verify no generated value satisfies a predicate
 * - find_any: find a value satisfying a predicate
 * - minimal: find the minimal shrunk value satisfying a predicate
 */

#include "hegel.h"

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace hegel::testing {

    /**
     * @brief Assert that a predicate holds for all generated values.
     *
     * Runs a Hegel test that draws values from the generator and checks
     * the predicate. If any value fails the predicate, the test fails
     * with a shrunk counterexample.
     *
     * @code{.cpp}
     * assert_all_examples(integers<int>({.min_value = 0}),
     *     [](int x) { return x >= 0; });
     * @endcode
     *
     * @throws std::runtime_error if a counterexample is found
     */
    template <typename T>
    void assert_all_examples(const generators::Generator<T>& gen,
                             std::function<bool(const T&)> predicate,
                             const options::HegelOptions& opts = {}) {

        hegel::hegel(
            [&]() {
                T value = draw(gen);
                if (!predicate(value)) {
                    throw std::runtime_error(
                        "Predicate failed for generated value");
                }
            },
            opts);
    }

    /**
     * @brief Assert that no generated value satisfies the predicate.
     *
     * Runs a Hegel test and verifies that the predicate is false for
     * every generated value. If any value satisfies the predicate,
     * the test fails.
     *
     * @code{.cpp}
     * assert_no_examples(integers<int>({.min_value = 1}),
     *     [](int x) { return x <= 0; });
     * @endcode
     *
     * @throws std::runtime_error if a satisfying example is found
     */
    template <typename T>
    void assert_no_examples(const generators::Generator<T>& gen,
                            std::function<bool(const T&)> predicate,
                            const options::HegelOptions& opts = {}) {

        hegel::hegel(
            [&]() {
                T value = draw(gen);
                if (predicate(value)) {
                    throw std::runtime_error(
                        "Found unexpected example satisfying predicate");
                }
            },
            opts);
    }

    /**
     * @brief Find any generated value satisfying the predicate.
     *
     * Runs a Hegel test, drawing values until one satisfies the predicate.
     * Returns the first satisfying value. Throws if no value is found
     * within the test case budget.
     *
     * @code{.cpp}
     * auto even = find_any(integers<int>(),
     *     [](int x) { return x % 2 == 0; });
     * @endcode
     *
     * @throws std::runtime_error if no satisfying value is found
     */
    template <typename T>
    T find_any(const generators::Generator<T>& gen,
               std::function<bool(const T&)> predicate,
               const options::HegelOptions& opts = {}) {

        std::optional<T> result;
        try {
            hegel::hegel(
                [&]() {
                    T value = draw(gen);
                    if (predicate(value)) {
                        result = std::move(value);
                        throw std::runtime_error("found");
                    }
                },
                opts);
        } catch (const std::runtime_error&) {
            if (result.has_value()) {
                return std::move(*result);
            }
        }

        if (result.has_value()) {
            return std::move(*result);
        }
        throw std::runtime_error(
            "Could not find any example satisfying the predicate");
    }

    /**
     * @brief Find the minimal shrunk value satisfying the predicate.
     *
     * Uses Hegel's shrinking to find the simplest value that satisfies
     * the predicate. This is useful for understanding the boundary
     * conditions of a property.
     *
     * @code{.cpp}
     * auto min_failing = minimal(integers<int>(),
     *     [](int x) { return x > 100; });
     * // min_failing == 101
     * @endcode
     *
     * @throws std::runtime_error if no satisfying value is found
     */
    template <typename T>
    T minimal(const generators::Generator<T>& gen,
              std::function<bool(const T&)> predicate,
              const options::HegelOptions& opts = {}) {

        std::optional<T> result;
        try {
            hegel::hegel(
                [&]() {
                    T value = draw(gen);
                    if (predicate(value)) {
                        result = value;
                        throw std::runtime_error("found");
                    }
                },
                opts);
        } catch (const std::runtime_error&) {
            // Expected - hegel shrinks and replays, updating result
        }

        if (result.has_value()) {
            return std::move(*result);
        }
        throw std::runtime_error(
            "Could not find any example satisfying the predicate");
    }

} // namespace hegel::testing
