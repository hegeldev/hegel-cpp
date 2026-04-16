#pragma once

/**
 * @file utils.h
 * @brief Test utilities for the Hegel C++ test suite.
 *
 * These mirror rust/tests/common/utils.rs. They are intentionally header-only
 * because they are templated on generator/value types.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <hegel/hegel.h>

namespace hegel::tests::common {

    namespace gs = hegel::generators;

    /// Sentinel exception used internally by `find_any` / `minimal` to bail
    /// out of a test case once an example matching the predicate is seen.
    /// Distinct types so the respective helpers can only catch their own
    /// signal.
    struct FindAnyFound : std::exception {
        const char* what() const noexcept override { return "FindAnyFound"; }
    };
    struct MinimalFound : std::exception {
        const char* what() const noexcept override { return "MinimalFound"; }
    };

    /// @brief Assert that `text` contains a regex match for `pattern`.
    inline void assert_matches_regex(const std::string& text,
                                     const std::string& pattern) {
        std::regex re(pattern);
        if (!std::regex_search(text, re)) {
            FAIL() << "Expected text to match regex: " << pattern
                   << "\nActual:\n"
                   << text;
        }
    }

    /// @brief Assert that every example drawn from `gen` satisfies `predicate`.
    template <typename T>
    void assert_all_examples(const gs::Generator<T>& gen,
                             std::function<bool(const T&)> predicate,
                             uint64_t test_cases = 100) {
        hegel::hegel(
            [&] {
                T value = hegel::draw(gen);
                if (!predicate(value)) {
                    throw std::runtime_error(
                        "Found value that does not match predicate");
                }
            },
            hegel::settings::HegelSettings{
                .test_cases = test_cases,
                .derandomize = true,
                .database = hegel::settings::Database::disabled()});
    }

    /// @brief Assert that no example drawn from `gen` satisfies `predicate`.
    template <typename T>
    void assert_no_examples(const gs::Generator<T>& gen,
                            std::function<bool(const T&)> predicate,
                            uint64_t test_cases = 100) {
        assert_all_examples<T>(
            gen, [predicate](const T& v) { return !predicate(v); }, test_cases);
    }

    namespace detail {
        /// Only swallow `hegel()`'s rethrow if it was caused by our own
        /// sentinel *and* carries the generic "Hegel test failed" message.
        /// Anything else — infrastructure failures, user code throwing its
        /// own exception — is re-raised.
        inline bool is_expected_property_failure(const std::exception& e,
                                                 bool sentinel_thrown) {
            return sentinel_thrown &&
                   std::string(e.what()) == "Hegel test failed";
        }
    } // namespace detail

    /// @brief Find any example satisfying `condition`, running up to
    /// `max_attempts` test cases.
    ///
    /// Throws `std::runtime_error` if no example is found within the budget.
    template <typename T>
    T find_any(const gs::Generator<T>& gen,
               std::function<bool(const T&)> condition,
               uint64_t max_attempts = 1000) {
        auto found = std::make_shared<std::optional<T>>();
        auto sentinel_thrown = std::make_shared<bool>(false);
        auto mu = std::make_shared<std::mutex>();

        try {
            hegel::hegel(
                [&] {
                    T value = hegel::draw(gen);
                    if (condition(value)) {
                        std::lock_guard<std::mutex> g(*mu);
                        *found = value;
                        *sentinel_thrown = true;
                        throw FindAnyFound{};
                    }
                },
                hegel::settings::HegelSettings{
                    .test_cases = max_attempts,
                    .derandomize = true,
                    .database = hegel::settings::Database::disabled()});
        } catch (const std::runtime_error& e) {
            if (!detail::is_expected_property_failure(e, *sentinel_thrown)) {
                throw;
            }
        }

        std::lock_guard<std::mutex> g(*mu);
        if (!found->has_value()) {
            throw std::runtime_error(
                "find_any: no example matched the predicate within " +
                std::to_string(max_attempts) + " attempts");
        }
        return found->value();
    }

    /// @brief Find the minimal (shrunk) example from `gen` that satisfies
    /// `condition`.
    ///
    /// Analogous to Hypothesis's `minimal()` test helper. Runs a property
    /// test where any value satisfying `condition` fails, letting Hegel shrink
    /// it to the minimal counterexample. Run is derandomized and uses no
    /// database.
    template <typename T>
    T minimal(const gs::Generator<T>& gen,
              std::function<bool(const T&)> condition,
              uint64_t test_cases = 500) {
        auto found = std::make_shared<std::optional<T>>();
        auto sentinel_thrown = std::make_shared<bool>(false);
        auto mu = std::make_shared<std::mutex>();

        try {
            hegel::hegel(
                [&] {
                    T value = hegel::draw(gen);
                    if (condition(value)) {
                        std::lock_guard<std::mutex> g(*mu);
                        *found = value;
                        *sentinel_thrown = true;
                        throw MinimalFound{};
                    }
                },
                hegel::settings::HegelSettings{
                    .test_cases = test_cases,
                    .derandomize = true,
                    .database = hegel::settings::Database::disabled()});
        } catch (const std::runtime_error& e) {
            if (!detail::is_expected_property_failure(e, *sentinel_thrown)) {
                throw;
            }
        }

        std::lock_guard<std::mutex> g(*mu);
        if (!found->has_value()) {
            throw std::runtime_error(
                "minimal: no example matched the predicate within " +
                std::to_string(test_cases) + " test cases");
        }
        return found->value();
    }

} // namespace hegel::tests::common
