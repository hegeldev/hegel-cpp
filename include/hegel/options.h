#pragma once

#include <cstdint>
#include <optional>
#include <string>

/** @namespace hegel::options
 * @brief The HegelOptions struct for configuring your Hegel run, and supporting
 * items
 */
namespace hegel::options {
    /**
     * @brief Verbosity levels for hegel output.
     */
    enum class Verbosity {
        Quiet,   /// Minimal output (used by TUI)
        Normal,  /// Default - standard test output
        Verbose, /// More detailed output
        Debug    /// Maximum verbosity + request/response logging
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
     * @see hegel::hegel()
     */
    struct HegelOptions {
        /// Number of test cases to run. Default: 100
        std::optional<uint64_t> test_cases;

        /// Verbosity level for hegel output. Default: Normal
        Verbosity verbosity = Verbosity::Normal;

        /// Path to the hegel binary. Default: "hegel" (uses PATH)
        std::optional<std::string> hegel_path;

        std::optional<uint64_t> seed;

        std::optional<std::string> failure_blob;

        bool print_blob;
    };
} // namespace hegel::options
