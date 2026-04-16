#pragma once

/**
 * @file internal.h
 * @brief Internal implementation details for Hegel
 *
 * Contains internal parts of the Hegel library that are referenced
 * in any of the header files shipped in the public API.
 *
 * Constructs that are not used in any of the header files,
 * i.e., are completely internal, are part of hegel::impl
 * and exist only in the src/ directory.
 */

#include <optional>
#include <stdexcept>
#include <string>

#include "json.h"

namespace hegel::impl::test_case {
    struct TestCaseData;
}

/// @cond INTERNAL
namespace hegel::internal {
    hegel::internal::json::json
    communicate_with_core(const hegel::internal::json::json& schema,
                          impl::test_case::TestCaseData* data);

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
     * auto person = hegel::draw(person_gen);
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

    /// @brief Get the current test case data, or nullptr if not in a test.
    impl::test_case::TestCaseData* get_test_case_data();

} // namespace hegel::internal
/// @endcond
