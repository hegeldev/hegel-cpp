#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
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
        Debug    ///< Maximum verbosity + engine-side shrinker tracing
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
     * @brief Phases of a property-test run.
     */
    enum class Phase {
        Explicit, ///< Run hard-coded explicit examples.
        Reuse,    ///< Replay counterexamples persisted in the database.
        Generate, ///< Randomly generate fresh test cases.
        Target,   ///< Hill-climb toward observed target scores.
        Shrink,   ///< Shrink failing examples toward minimal counterexamples.
    };

    /**
     * @brief All phases, the default for Settings::phases.
     * @return A vector containing every Phase variant.
     */
    inline std::vector<Phase> all_phases() {
        return {Phase::Explicit, Phase::Reuse, Phase::Generate, Phase::Target,
                Phase::Shrink};
    }

    /**
     * @brief How much of a run the engine performs.
     */
    enum class Mode {
        TestRun,        ///< Full property-test run. The default.
        SingleTestCase, ///< Produce exactly one test case and stop, with no
                        ///< shrinking. Useful for exploratory probes.
    };

    /**
     * @brief Source of randomness the engine draws from.
     */
    enum class Backend {
        Auto,    ///< Choose automatically (the default): Urandom when running
                 ///< inside Antithesis, Default otherwise.
        Default, ///< Expand a single seeded PRNG. Runs are reproducible from
                 ///< the seed and shrinking / replay work as usual.
        Urandom, ///< Read fresh entropy from `/dev/urandom` on every draw.
                 ///< Intended for running under Antithesis; you almost
                 ///< certainly don't want it otherwise.
    };

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

    /// @cond INTERNAL
    namespace internal {
        // True if a CI environment is detected, by probing the variables the
        // common CI providers set. Used as the default for
        // Settings::derandomize so runs are reproducible under CI.
        inline bool in_ci() {
            // expected == nullptr means "any value present satisfies it".
            struct CiVar {
                const char* name;
                const char* expected;
            };
            static constexpr CiVar ci_vars[] = {
                {"CI", nullptr},
                {"TF_BUILD", "true"},
                {"BUILDKITE", "true"},
                {"CIRCLECI", "true"},
                {"CIRRUS_CI", "true"},
                {"CODEBUILD_BUILD_ID", nullptr},
                {"GITHUB_ACTIONS", "true"},
                {"GITLAB_CI", nullptr},
                {"HEROKU_TEST_RUN_ID", nullptr},
                {"TEAMCITY_VERSION", nullptr},
            };
            for (const CiVar& v : ci_vars) {
                const char* value = std::getenv(v.name);
                if (value == nullptr) {
                    continue;
                }
                if (v.expected == nullptr ||
                    std::strcmp(value, v.expected) == 0) {
                    return true;
                }
            }
            return false;
        }
    } // namespace internal
    /// @endcond

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
        /// across executions. Defaults to true when a CI environment is
        /// detected, false otherwise.
        bool derandomize = internal::in_ci();

        /// If true, keep generating after the first failure to surface
        /// additional distinct bugs, and report all of them. If false (the
        /// default), stop the run at the first failing example.
        bool report_multiple_failures = false;

        /// If true (the default), a failure report ends with a
        /// `rerun with:` line holding the base64 blob that replays the
        /// failure.
        bool print_blob = true;

        /// Configure the Hegel database. See Database. Defaults to a database
        /// at `.hegel`, or disabled when a CI environment is detected.
        Database database =
            internal::in_ci() ? Database::disabled() : Database::unset();

        /// Key scoping the examples stored in and replayed from the database.
        /// Unset (the default) disables persistence and Reuse-phase replay.
        std::optional<std::string> database_key;

        /// Health checks to suppress for this test.
        std::vector<HealthCheck> suppress_health_check;

        /// Phases to run. Defaults to all phases; phases left out are
        /// disabled (e.g. drop Phase::Shrink to keep failing examples
        /// unshrunk). An empty vector produces a run that does nothing.
        std::vector<Phase> phases = all_phases();

        /// How much of a run to perform. Defaults to Mode::TestRun.
        Mode mode = Mode::TestRun;

        /// Randomness backend. Defaults to Backend::Auto; picking an explicit
        /// backend overrides the automatic detection.
        Backend backend = Backend::Auto;

        /// The maximum number of steps a stateful test case attempts. Each
        /// case runs at least one step and at most this many. Must be at
        /// least 1.
        int64_t stateful_step_count = 50;
    };
} // namespace hegel
