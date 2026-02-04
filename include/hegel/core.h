#pragma once

/**
 * @file core.h
 * @brief Core types and functions for Hegel SDK
 *
 * Contains Mode enum, HegelOptions, HegelReject exception, and
 * fundamental functions like assume() and note().
 */

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace hegel {

    // =============================================================================
    // State
    // =============================================================================

    /**
    * @brief Print a note message for debugging.
    *
    * Only prints on the last run (final replay for counterexample output)
    * to avoid cluttering output during the many test iterations.
    *
    * @param message The message to print
    */
    void note(const std::string& message);

    // =============================================================================
    // Embedded Mode Options
    // =============================================================================

    /**
    * @brief Verbosity levels for hegel output.
    */
    enum class Verbosity {
        /// Minimal output (used by TUI)
        Quiet,
        /// Default - standard test output
        Normal,
        /// More detailed output
        Verbose,
        /// Maximum verbosity + request/response logging
        Debug
    };

    /**
    * @brief Convert Verbosity enum to command-line string.
    * @param v The verbosity level
    * @return The string representation for CLI argument
    */
    inline const char* verbosity_to_string(Verbosity v) {
        switch (v) {
            case Verbosity::Quiet:
                return "quiet";
            case Verbosity::Normal:
                return "normal";
            case Verbosity::Verbose:
                return "verbose";
            case Verbosity::Debug:
                return "debug";
        }
        return "normal";  // Default fallback
    }

    /**
    * @brief Configuration options for embedded mode execution.
    * @see hegel()
    */
    struct HegelOptions {
        /// Number of test cases to run. Default: 100
        std::optional<uint64_t> test_cases;

        /// Verbosity level for hegel output. Default: Normal
        Verbosity verbosity = Verbosity::Normal;

        /// Path to the hegel binary. Default: "hegel" (uses PATH)
        std::optional<std::string> hegel_path;
    };

    // =============================================================================
    // Embedded Mode Exception
    // =============================================================================

    /**
    * @brief Exception thrown when a test case is rejected.
    *
    * Thrown by assume(false) in embedded mode to signal that the current
    * test case should be discarded rather than counted as a failure.
    * This is caught by the hegel() function's test runner.
    */
    class HegelReject : public std::exception {
    public:
        const char* what() const noexcept override { return "assume failed"; }
    };

    // =============================================================================
    // Core API Functions
    // =============================================================================

    /**
    * @brief Stop the current test case immediately.
    *
    * Throws HegelReject which is caught by hegel().
    *
    * @note This function never returns.
    */
    [[noreturn]] void stop_test();

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

}  // namespace hegel
