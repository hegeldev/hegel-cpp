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

        // Construct in artificial mode. The referenced TestCase must outlive
        // this HegelRandom (typically both live for the duration of one test
        // callback invocation).
        explicit HegelRandom(const TestCase& tc);

        // Construct in true-random mode
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
