#pragma once

/**
 * @file options.h
 * @brief HegelOptions and supporting classes
 */

#include <cstdint>
#include <optional>
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
}  // namespace hegel
