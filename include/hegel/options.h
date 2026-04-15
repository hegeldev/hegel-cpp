#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

    enum class HealthCheck {
        FilterTooMuch,        /// Too many test cases are being filtered out via
                              /// assume()
        TooSlow,              /// Test execution is too slow
        TestCasesTooLarge,    /// Generated test cases are too large
        LargeInitialTestCase, /// The smallest natural input is very large
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
     * @brief Convert HealthCheck enum to command-line string.
     * @param hc The health check
     * @return The string representation for CLI argument
     */
    inline const char* health_check_to_string(HealthCheck hc) {
        switch (hc) {
        case HealthCheck::FilterTooMuch:
            return "filter_too_much";
        case HealthCheck::TooSlow:
            return "too_slow";
        case HealthCheck::TestCasesTooLarge:
            return "test_cases_too_large";
        case HealthCheck::LargeInitialTestCase:
            return "large_initial_test_case";
        }
    }

    inline const std::vector<HealthCheck> all_health_checks = {
        HealthCheck::FilterTooMuch,
        HealthCheck::TooSlow,
        HealthCheck::TestCasesTooLarge,
        HealthCheck::LargeInitialTestCase,
    };

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

        std::vector<HealthCheck> suppress_health_check;
    };
} // namespace hegel::options
