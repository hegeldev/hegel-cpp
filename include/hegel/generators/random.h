#pragma once

/**
 * @file random.h
 * @brief Random engine integration with the C++ <random> library
 *
 * Provides HegelRandom, a class satisfying UniformRandomBitGenerator,
 * so it can be used with any std::random distribution (e.g.,
 * std::lognormal_distribution, std::normal_distribution).
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
            false; ///< If true, use a seeded local PRNG instead of
                   ///< per-value Hypothesis requests
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
     * Two modes:
     * - **Artificial (default)**: Each `operator()` call sends a generate
     *   request to hegeld, enabling per-value shrinking.
     *   Artificial mode may cause the following Hypothesis health check failure:
     * 
     *   "The smallest natural input for this test is very large. 
     *   This makes it difficult for Hypothesis to generate good inputs, 
     *   especially when trying to shrink failing inputs"
     * 
     *   when used with distributions implemented via rejection sampling, such as
     *   std::normal_distribution. Alternatively, you may set use_true_random = true
     *   to continue using distributions from the standard library, but if you require
     *   artificial randomness, you must use the Hegel version of the distribution.
     * - **True random**: A single seed is generated via Hypothesis at
     *   construction time, then all calls use a local `std::mt19937`.
     *   Faster but only the seed shrinks.
     *
     * @code{.cpp}
     *  auto rng = randoms().generate();
     *  // Use with non-rejection sampling based <random> distribution
     *  std::uniform_real_distribution<double> dist(0.0, 10.0);
     *  double uniform_value = dist(rng);
     *  // Using true random
     *  auto rng = randoms({ .use_true_random = true }).generate();
     *  std::lognormal_distribution<double> dist(0.0, 10.0);
     *  double uniform_value = dist(rng);
     * @endcode
     */
    class HegelRandom {
      public:
        using result_type = uint32_t;

        /// @brief Construct in artificial mode (per-value requests to hegeld)
        HegelRandom();

        /// @brief Construct in true-random mode with the given seed
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
        std::optional<std::mt19937> engine_;
    };

    // =============================================================================
    // Factory function
    // =============================================================================

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
     * auto rng = randoms().generate();
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
