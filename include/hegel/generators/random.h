#pragma once

#include <cstdint>
#include <optional>
#include <random>

#include "hegel/core.h"
#include "hegel/generators/numeric.h"

namespace hegel::generators {

    /**
     * @brief Parameters for randoms() generator.
     */
    struct RandomsParams {
        bool use_true_random =
            false; ///< If false (the default), every `operator()` call is an
                   ///< engine draw: the values become part of the test case's
                   ///< choice sequence, so Hegel can steer them toward
                   ///< interesting cases and shrink them on failure. If true,
                   ///< Hegel draws a single seed and the values come from a
                   ///< local `std::mt19937` seeded with it: reproducible via
                   ///< the seed, but the individual values are not shrunk.
                   ///< Use true-random mode when passing the engine to
                   ///< `<random>` distributions or when your code relies on
                   ///< the outputs being uniformly distributed.
    };

    /**
     * @brief A random engine whose output Hegel controls.
     *
     * Produced by randoms(); see it for how to choose between the two modes.
     * Satisfies the C++ UniformRandomBitGenerator named requirement.
     *
     * @warning **Lifetime:** in the default (test-case-backed) mode a
     * HegelRandom holds a pointer to the TestCase it was drawn from, which is
     * only valid while the test-case callback is running. Do not store one in
     * a global, a member, or anything else that outlives the callback — calls
     * after the callback returns are use-after-free. (True-random mode is
     * self-contained and has no such constraint.)
     *
     * @code{.cpp}
     *  // Default mode: each call is a shrinkable engine draw.
     *  auto rng = tc.draw(gs::randoms());
     *  uint32_t bits = rng();
     *
     *  // True-random mode: a seeded local PRNG, e.g. for <random>
     *  // distributions.
     *  auto rng2 = tc.draw(gs::randoms({.use_true_random = true}));
     *  std::lognormal_distribution<double> dist(0.0, 10.0);
     *  double value = dist(rng2);
     * @endcode
     */
    class HegelRandom {
      public:
        /// @cond INTERNAL
        using result_type = uint32_t;

        /**
         * @brief Construct in the default (test-case-backed) mode.
         *
         * Each call to `operator()` draws a `uint32_t` from the engine
         * through @p tc, so the values join the choice sequence and can be
         * shrunk.
         *
         * @param tc The active test case (non-owning). The constructed
         *           HegelRandom must not outlive the test-case callback.
         */
        explicit HegelRandom(const TestCase& tc);

        /**
         * @brief Construct in true-random mode using a seeded local PRNG.
         *
         * Values are produced by an internal `std::mt19937` seeded with
         * @p seed; they do not touch the engine, so they are reproducible
         * from the seed but not individually shrunk.
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

    /// @name Random
    /// @{

    /**
     * @brief Generate random number generators.
     *
     * Returns a Generator producing HegelRandom instances, for testing code
     * that takes a UniformRandomBitGenerator as input. There are two modes:
     *
     * - **Default** (`use_true_random = false`): every call to the returned
     *   engine's `operator()` is an engine draw. The values are part of the
     *   test case's choice sequence, so Hegel steers them toward interesting
     *   cases and shrinks them when the test fails. Prefer this mode when the
     *   code under test consumes the 32-bit outputs directly — it tends to
     *   find bugs that uniformly random values would hit only with very low
     *   probability.
     * - **True-random** (`use_true_random = true`): Hegel draws one seed and
     *   the engine expands it with a local `std::mt19937`. Runs are
     *   reproducible via the seed, but the individual values are not shrunk.
     *
     * If you pass the engine to a `<random>` distribution
     * (`std::uniform_real_distribution`, `std::lognormal_distribution`, ...),
     * use true-random mode. Distributions expect uniform bits and consume a
     * variable number of them; feeding them Hegel's engine-controlled (and,
     * during shrinking, deliberately non-uniform) draws distorts their output
     * and can make rejection-sampling loops behave pathologically, up to
     * hanging. Also use true-random mode whenever your code relies on the
     * distribution of the values for correctness.
     *
     * @warning In the default mode the drawn HegelRandom must not outlive the
     * test-case callback; see HegelRandom for the lifetime rules.
     *
     * @code{.cpp}
     * namespace gs = hegel::generators;
     * auto rng = tc.draw(gs::randoms({.use_true_random = true}));
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
