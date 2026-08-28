#pragma once

#include "test_case.h"

#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// @cond INTERNAL
// opaque libhegel handle types.
struct hegel_collection_t;
struct hegel_pool_t;
struct hegel_state_machine_t;
/// @endcond

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
        SampledFrom = 14,
        EnumVariant = 15,
        StatefulRule = 31
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
    bool draw_boolean(const TestCase& tc, double p,
                      std::optional<bool> forced = std::nullopt);
    double draw_float(const TestCase& tc, uint32_t width, double min_value,
                      double max_value, bool allow_nan, bool allow_infinity,
                      bool exclude_min, bool exclude_max,
                      double smallest_nonzero_magnitude);

    inline constexpr uint64_t no_max_size = ~uint64_t{0};
    class CollectionHandle {
      public:
        CollectionHandle(const TestCase& tc, uint64_t min_size,
                         uint64_t max_size);
        ~CollectionHandle();
        CollectionHandle(const CollectionHandle&) = delete;
        CollectionHandle& operator=(const CollectionHandle&) = delete;

        bool more(const TestCase& tc);

        void reject(const TestCase& tc, const char* why);

      private:
        hegel_collection_t* handle_ = nullptr;
    };

    class PoolHandle {
      public:
        explicit PoolHandle(const TestCase& tc);
        ~PoolHandle();
        PoolHandle(const PoolHandle&) = delete;
        PoolHandle& operator=(const PoolHandle&) = delete;

        // Returns a fresh variable id.
        int64_t add(const TestCase& tc);
        // Returns the variable id the engine picked, removing it from the
        // pool when consume is true.
        int64_t draw_variable(const TestCase& tc, bool consume);

      private:
        hegel_pool_t* handle_ = nullptr;
    };

    inline constexpr int64_t state_machine_done = -1;
    class StateMachineHandle {
      public:
        StateMachineHandle(const TestCase& tc,
                           const std::vector<std::string>& rule_names,
                           const std::vector<std::string>& invariant_names);
        ~StateMachineHandle();
        StateMachineHandle(const StateMachineHandle&) = delete;
        StateMachineHandle& operator=(const StateMachineHandle&) = delete;

        // Returns the index of the next rule to run, or state_machine_done
        // once the engine's step budget for this test case is used up.
        int64_t next_rule(const TestCase& tc);
        // Report that an assumption failed in the last rule, so it does not
        // count against the step budget.
        void rule_rejected(const TestCase& tc);

      private:
        hegel_state_machine_t* handle_ = nullptr;
    };

    class NoteIndentScope {
      public:
        explicit NoteIndentScope(const TestCase& tc);
        ~NoteIndentScope();
        NoteIndentScope(const NoteIndentScope&) = delete;
        NoteIndentScope& operator=(const NoteIndentScope&) = delete;

      private:
        const TestCase& tc_;
    };

    class DrawLogScope {
      public:
        DrawLogScope(const TestCase& tc, std::string_view name,
                     bool repeatable);
        ~DrawLogScope();
        DrawLogScope(const DrawLogScope&) = delete;
        DrawLogScope& operator=(const DrawLogScope&) = delete;

        bool should_log() const;

        void log(const std::string& rendered) const;

      private:
        const TestCase& tc_;
        bool outermost_;
        std::string display_;
    };
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
