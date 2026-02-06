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

#include <string>
#include <stdexcept>
#include <optional>


/// @cond INTERNAL
namespace hegel::internal {
    std::string communicate_with_socket(const std::string& schema);

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

    // =============================================================================
    // Response wrapper for socket communication
    // =============================================================================
    template <typename T>
    struct Response {
        std::optional<T> result;
        std::optional<std::string> error;
    };
}
/// @endcond
