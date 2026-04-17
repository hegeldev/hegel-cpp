#pragma once

/**
 * @file random.h
 * @brief Random engine integration with the C++ `<random>` library
 *
 * Provides HegelRandom, a class satisfying UniformRandomBitGenerator,
 * so it can be used with any std::random distribution.
 */

#include <cstdint>
#include <optional>
#include <random>

#include "hegel/core.h"
#include "hegel/generators/numeric.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for randoms() strategy.
     */
    struct RandomsParams {
        bool use_true_random =
            false; ///< If true, use a local PRNG seed by Hegel instead of
                   ///< per-value Hypothesis requests. Use this mode when the
                   ///< cost of routing every draw through Hypothesis would be
                   ///< too expensive. Values produced in true-random mode
                   ///< cannot be shrunk.
    };

    // =============================================================================
    // HegelRandom class
    // =============================================================================

    /**
     * @brief A random engine that integrates with Hypothesis via Hegel.
     *
     * Satisfies the C++ UniformRandomBitGenerator named requirement, so it
     * can be used with any `<random>` distribution.
     *
     * @code{.cpp}
     *  auto rng = tc.draw(gs::randoms());
     *  std::uniform_real_distribution<double> dist(0.0, 10.0);
     *  double uniform_value = dist(rng);
     *
     *  // Using true random
     *  auto rng = tc.draw(gs::randoms({ .use_true_random = true }));
     *  std::lognormal_distribution<double> dist(0.0, 10.0);
     *  double uniform_value = dist(rng);
     * @endcode
     */
    class HegelRandom {
      public:
        /// @cond INTERNAL
        using result_type = uint32_t;

        /**
         * @brief Construct in artificial (Hegel-backed) mode.
         *
         * Each call to `operator()` draws entropy from Hegel via the
         * given test-case data, so the resulting values can be shrunken.
         * The referenced TestCase must outlive this HegelRandom (typically both
         * live for the duration of one test callback invocation).
         *
         * @param data The active test case's data stream (non-owning).
         */
        explicit HegelRandom(const TestCase& tc);

        /**
         * @brief Construct in true-random mode using a seeded local PRNG.
         *
         * Values are produced by an internal `std::mt19937` seeded with
         * @p seed. Values produced in true-random mode cannot be shrunk.
         *
         * @param seed Seed for the internal Mersenne Twister engine.
         */
        explicit HegelRandom(uint64_t seed);

        static constexpr result_type min() {
            return std::numeric_limits<result_type>::min();
        }

        static constexpr result_type max() {
            return std::numeric_limits<result_type>::max();
        }

        /// @brief Generate a random uint32_t value
        result_type operator()();
        /// @endcond

      private:
        const TestCase* tc_ = nullptr;
        std::optional<std::mt19937> engine_;
    };

    /// @name Random Strategies
    /// @{

    /**
     * @brief Generate random number generators.
     *
     * Returns a Generator that produces HegelRandom instances satisfying
     * UniformRandomBitGenerator, enabling use with any `<random>` distribution.
     * If use_true_random is set to true then values will be drawn from their
     * usual distribution, seeded by Hegel. Otherwise they will actually be
     * Hegel generated values (and will be shrunk accordingly for any failing
     * test case). Setting use_true_random=false will tend to expose bugs that
     * would occur with very low probability when it is set to true, and this
     * flag should only be set to true when your code relies on the distribution
     * of values for correctness.
     *
     * @note C++ distributions implemented using rejection sampling, such as
     * std::normal_distribution, std::lognormal_distribution, and
     * std::poisson_distribution for mean >= 10, and
     * std::gamma_distribution, must use true random mode.
     *
     *
     * @code{.cpp}
     * namespace gs = hegel::generators;
     * auto rng = tc.draw(gs::randoms());
     *
     * std::lognormal_distribution<double> dist(0.0, 1.0);
     * double value = dist(rng);
     * @endcode
     *
     * @param params Configuration (use_true_random to switch modes)
     * @return Generator producing HegelRandom instances
     */
    Generator<HegelRandom> randoms(RandomsParams params = {});

    /// @}

} // namespace hegel::generators
