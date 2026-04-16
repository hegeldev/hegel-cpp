#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/** @namespace hegel::settings
 * @brief The HegelSettings struct for configuring your Hegel run, and
 * supporting items
 */
namespace hegel::settings {
    /**
     * @brief Verbosity levels for hegel output.
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
     * @brief Hypothesis health checks that can be suppressed.
     *
     * Wire names match what the Hegel server (Hypothesis) expects.
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
     * @brief Example database configuration.
     *
     * A three-state value for HegelSettings::database:
     *
     *  - `Database::unset()` — let the server pick the database
     *    location. This is the default.
     *  - `Database::disabled()` — no database is used for this run.
     *    Sends `database: null` on the wire.
     *  - `Database::from_path(path)` — use the given directory as the
     *    example database.
     */
    class Database {
      public:
        /// Which variant this Database represents.
        enum class Kind {
            Unset,    ///< Server picks the default database location.
            Disabled, ///< No database is used for this run.
            Path,     ///< A specific directory is used for the database.
        };

        /// Let the server pick the default database location.
        /// @return A Database in the Unset state.
        static Database unset() { return {Kind::Unset, {}}; }

        /// Disable the example database for this run.
        /// @return A Database in the Disabled state.
        static Database disabled() { return {Kind::Disabled, {}}; }

        /// Use the given directory as the example database.
        /// @param path Directory path for the example database.
        /// @return A Database in the Path state.
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
     * @brief Configuration options for embedded mode execution.
     * @see hegel::hegel()
     */
    struct HegelSettings {
        /// Number of test cases to run. Default: 100
        std::optional<uint64_t> test_cases;

        /// Verbosity level for hegel output. Default: Normal
        Verbosity verbosity = Verbosity::Normal;

        /// Explicit RNG seed. If unset, Hegel picks one (unless derandomize).
        std::optional<uint64_t> seed;

        /// If true, the server uses a derandomized (deterministic) RNG.
        /// Combined with `database = Database::disabled()` this produces a
        /// fully reproducible run, which is what test helpers like
        /// `minimal()` want.
        bool derandomize = false;

        /// Example database configuration. Default: `Database::unset()`
        /// (server picks the default location).
        ///
        /// Use `Database::disabled()` to disable the database for this
        /// run, or `Database::from_path()` to use a specific directory.
        Database database = Database::unset();

        /// Health checks to suppress for this run. Defaults to none.
        std::vector<HealthCheck> suppress_health_check;
    };
} // namespace hegel::settings
