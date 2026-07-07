#pragma once

#include <cstdint>
#include <exception>

namespace hegel {
    class TestCase;
}

/**
 * @brief Internal utilities exposed in public headers.
 */
namespace hegel::internal {
    /// @cond INTERNAL

    /* Typed draw primitives over libhegel's C ABI. Declared here (rather
     * than in a private header) because the generator templates in the
     * public headers call them from their do_draw implementations. Each
     * throws HegelStopTest when the engine's choice budget is exhausted,
     * HegelReject when the draw rejects itself, and std::runtime_error on
     * any other engine failure.
     */

    // Span labels understood by the engine's shrinker. Values mirror the
    // C ABI's hegel_label_t (static_asserted in src/engine.cpp).
    enum class SpanLabel : uint64_t {
        List = 1,
        ListElement = 2,
        Set = 3,
        SetElement = 4,
        Map = 5,
        MapEntry = 6,
        Tuple = 7,
        OneOf = 8,
        Optional = 9,
        FlatMap = 11,
        Filter = 12,
        Mapped = 13,
    };

    // Open / close a labeled span around a group of draws so the shrinker
    // can treat them as a unit. stop_span(tc, true) marks the span rejected
    // (e.g. a filter predicate failed) so the engine retries from before it.
    void start_span(const TestCase& tc, SpanLabel label);
    void stop_span(const TestCase& tc, bool discard = false);

    int64_t draw_integer(const TestCase& tc, int64_t min_value,
                         int64_t max_value);
    // Uses the engine's big-integer draw when the range exceeds int64_t.
    uint64_t draw_integer_unsigned(const TestCase& tc, uint64_t min_value,
                                   uint64_t max_value);
    bool draw_boolean(const TestCase& tc, double p);
    double draw_float(const TestCase& tc, uint32_t width, double min_value,
                      double max_value, bool allow_nan, bool allow_infinity,
                      bool exclude_min, bool exclude_max,
                      double smallest_nonzero_magnitude);

    // Engine-managed variable-length collections: the engine decides how
    // many elements to produce; the caller loops on collection_more and
    // draws one element per `true`. Pass no_max_size for no upper bound.
    inline constexpr uint64_t no_max_size = ~uint64_t{0};
    int64_t new_collection(const TestCase& tc, uint64_t min_size,
                           uint64_t max_size);
    bool collection_more(const TestCase& tc, int64_t collection_id);
    // Tell the engine the last element produced is unacceptable (e.g. a
    // duplicate in a set) so it tries a different one.
    void collection_reject(const TestCase& tc, int64_t collection_id,
                           const char* why);

    /* Exception thrown when a test case is rejected and should be
     * discarded (e.g. by `TestCase::assume(false)`, an exhausted
     * `filter()`, or an `UnsatisfiedAssumption` from the engine).
     * The runner records the case as INVALID and continues.
     */
    class HegelReject : public std::exception {
      public:
        const char* what() const noexcept override {
            return "test case rejected";
        }
    };

    /* Exception thrown when the backend tells us to abandon the current
     * test iteration entirely (StopTest, Overflow, FlakyStrategyDefinition,
     * FlakyReplay). The runner unwinds the test body and skips
     * `mark_complete` — the engine already knows the iteration is over.
     */
    class HegelStopTest : public std::exception {
      public:
        const char* what() const noexcept override {
            return "test case stopped by backend";
        }
    };

} // namespace hegel::internal

/// @endcond
