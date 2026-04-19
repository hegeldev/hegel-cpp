#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hegel {
    /**
     * @brief Verbosity levels.
     */
    enum class Verbosity {
        Quiet,   ///< Minimal output (used by TUI)
        Normal,  ///< Default - standard test output
        Verbose, ///< More detailed output
        Debug    ///< Maximum verbosity + request/response logging
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
     * @brief Health checks.
     */
    enum class HealthCheck {
        FilterTooMuch,        ///< Test filters out too many generated examples.
        TooSlow,              ///< Test cases are running too slowly.
        TestCasesTooLarge,    ///< Generated test cases are too large.
        LargeInitialTestCase, ///< First generated test case is unusually large.
    };

    /// @cond INTERNAL
    inline const char* health_check_to_string(HealthCheck c) {
        switch (c) {
        case HealthCheck::FilterTooMuch:
            return "filter_too_much";
        case HealthCheck::TooSlow:
            return "too_slow";
        case HealthCheck::TestCasesTooLarge:
            return "test_cases_too_large";
        case HealthCheck::LargeInitialTestCase:
            return "large_initial_test_case";
        }
        return "";
    }
    /// @endcond

    /**
     * @brief All health checks, suitable for full suppression.
     * @return A vector containing every HealthCheck variant.
     */
    inline std::vector<HealthCheck> all_health_checks() {
        return {HealthCheck::FilterTooMuch, HealthCheck::TooSlow,
                HealthCheck::TestCasesTooLarge,
                HealthCheck::LargeInitialTestCase};
    }

    /**
     * @brief Configure the Hegel database.
     */
    class Database {
      public:
        /// The configuration of the database. See the methods below.
        enum class Kind {
            Unset, ///< Default behavior. By default, Hegel places the database
                   ///< at `.hegel`.
            Disabled, ///< Disable the database.
            Path,     ///< Use the given directory as the database.
        };

        /// Default behavior. By default, Hegel places the database at `.hegel`.
        /// @return A database with Database.Kind.Unset.
        static Database unset() { return {Kind::Unset, {}}; }

        /// Disable the database.
        /// @return A database with Database.Kind.Disabled.
        static Database disabled() { return {Kind::Disabled, {}}; }

        /// Use the given directory as the database.
        /// @param path The directory path for the database.
        /// @return A database with Database.Kind.Path.
        static Database from_path(std::string path) {
            return {Kind::Path, std::move(path)};
        }

        /// @cond INTERNAL
        Kind kind() const { return kind_; }
        const std::string& path() const { return path_; }
        /// @endcond

      private:
        Database(Kind k, std::string p) : kind_(k), path_(std::move(p)) {}
        Kind kind_;
        std::string path_;
    };

    /**
     * @brief Configuration options for hegel::test().
     */
    struct Settings {
        /// Number of test cases to run. Defaults to 100.
        std::optional<uint64_t> test_cases;

        /// Verbosity level. Defaults to Verbosity::Normal.
        Verbosity verbosity = Verbosity::Normal;

        /// Explicit seed for the test.
        std::optional<uint64_t> seed;

        /// If true, use a deterministic RNG, making the test deterministic
        /// across executions.
        bool derandomize = false;

        /// Configure the Hegel database. See Database. Defaults to a database
        /// at `.hegel`.
        Database database = Database::unset();

        /// Health checks to suppress for this test.
        std::vector<HealthCheck> suppress_health_check;
    };
} // namespace hegel
