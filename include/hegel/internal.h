#pragma once

/**
 * @file internal.h
 * @brief Internal implementation details for Hegel SDK
 *
 * Contains internal parts of the Hegel SDK that are referenced
 * in any of the header files shipped in the public API.
 *
 * Constructs that are not used in any of the header files,
 * i.e., are completely internal, are part of hegel::impl
 * and exist only in the src/ directory.
 */

#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

/// @cond INTERNAL
namespace hegel::internal {
    nlohmann::json communicate_with_socket(const nlohmann::json& schema);

    /* Print a note message for debugging.
     *
     * Only prints on the last run (final replay for counterexample output)
     * to avoid cluttering output during the many test iterations.
     */
    void note(const std::string& message);

    /**
     * @brief Assume a condition is true; reject if false.
     *
     * Use this when generated data doesn't meet test preconditions.
     * This signals to Hegel that the input is invalid and should be
     * discarded, not counted as a test failure.
     *
     * @code{.cpp}
     * auto person = person_gen.generate();
     * hegel::assume(person.age >= 18);  // Only test adults
     * // ... rest of test
     * @endcode
     *
     * @param condition The condition to check
     */
    void assume(bool condition);

    /* Exception thrown when a test case is rejected
     * and should be discarded rather than counted as a failure.
     */
    class HegelReject : public std::exception {
      public:
        const char* what() const noexcept override { return "assume failed"; }
    };

    /**
     * @brief Stop the current test case immediately.
     *
     * Throws HegelReject which is caught by hegel().
     *
     * @note This function never returns.
     */
    [[noreturn]] void stop_test();

    /**
     * @brief Start a generation span for better shrinking.
     *
     * Spans group related generation calls, helping Hypothesis
     * understand data structure for effective shrinking.
     *
     * @param label Numeric span label (see labels namespace)
     */
    void start_span(uint64_t label);

    /**
     * @brief End the current generation span.
     *
     * @param discard If true, tells Hypothesis this span's data
     *        should be discarded (e.g., because a filter rejected it)
     */
    void stop_span(bool discard = false);

    /**
     * @brief Guide the test engine toward higher values of a metric.
     *
     * @param value The numeric value to maximize
     * @param label A label for this target metric
     */
    void target(double value, const std::string& label = "");

    /// Span label constants for generation structure tracking.
    namespace labels {
        constexpr uint64_t LIST = 1;
        constexpr uint64_t LIST_ELEMENT = 2;
        constexpr uint64_t SET = 3;
        constexpr uint64_t SET_ELEMENT = 4;
        constexpr uint64_t MAP = 5;
        constexpr uint64_t MAP_ENTRY = 6;
        constexpr uint64_t TUPLE = 7;
        constexpr uint64_t ONE_OF = 8;
        constexpr uint64_t OPTIONAL = 9;
        constexpr uint64_t FIXED_DICT = 10;
        constexpr uint64_t FLAT_MAP = 11;
        constexpr uint64_t FILTER = 12;
        constexpr uint64_t MAPPED = 13;
        constexpr uint64_t SAMPLED_FROM = 14;
        constexpr uint64_t ENUM_VARIANT = 15;
    } // namespace labels

} // namespace hegel::internal
/// @endcond
