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
    // Options
    // =============================================================================

    /**
    * @brief Verbosity levels for hegel output.
    */
    enum class Verbosity {
        Quiet,      /// Minimal output (used by TUI)
        Normal,     /// Default - standard test output
        Verbose,    /// More detailed output
        Debug       /// Maximum verbosity + request/response logging
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
            case Verbosity::Verbose:
                return "verbose";
            case Verbosity::Debug:
                return "debug";
            case Verbosity::Normal:
            default:
                return "normal";            
        }
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
    // Core API Functions
    // =============================================================================

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
