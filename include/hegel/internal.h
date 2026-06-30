#pragma once

#include <exception>

#include "json.h"

namespace hegel {
    class TestCase;
}

/**
 * @brief Internal utilities exposed in public headers.
 */
namespace hegel::internal {
    /// @cond INTERNAL
    hegel::internal::json::json
    generate_from_schema(const hegel::internal::json::json& schema,
                         const hegel::TestCase& tc);

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
