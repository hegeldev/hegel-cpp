#pragma once

/**
 * @file random.h
 * @brief Random engine integration with the C++ <random> library
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
                   ///< cost of routing every draw through Hypothesis would be too expensive.
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
     *  auto rng = hegel::draw(randoms());
     *  std::uniform_real_distribution<double> dist(0.0, 10.0);
     *  double uniform_value = dist(rng);
     *
     *  // Using true random
     *  auto rng = hegel::draw(randoms({ .use_true_random = true }));
     *  std::lognormal_distribution<double> dist(0.0, 10.0);
     *  double uniform_value = dist(rng);
     * @endcode
     */
    class HegelRandom {
      public:
        using result_type = uint32_t;

        /**
         * @brief Construct in artificial (Hypothesis-backed) mode.
         *
         * Each call to `operator()` draws entropy from Hypothesis via the
         * given test-case data, so the resulting values can be shrunken. 
         *
         * @param data The active test case's data stream (non-owning).
         */
        explicit HegelRandom(impl::data::TestCaseData* data);

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

      private:
        impl::data::TestCaseData* data_ = nullptr;
        std::optional<std::mt19937> engine_;
    };

    /// @name Random Strategies
    /// @{

    /**
     * @brief Generate random number generators.
     *
     * Returns a Generator that produces HegelRandom instances satisfying
     * UniformRandomBitGenerator, enabling use with any `<random>` distribution.
     *
     * @code{.cpp}
     * using namespace hegel::generators;
     * auto rng = hegel::draw(randoms());
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
