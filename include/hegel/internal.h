#pragma once

/**
 * @file internal.h
 * @brief Internal implementation details for Hegel
 *
 * Contains internal parts of the Hegel library that are referenced
 * in any of the header files shipped in the public API.
 *
 * Constructs that are not used in any of the header files,
 * i.e., are completely internal, are part of hegel::impl
 * and exist only in the src/ directory.
 */

#include <exception>

#include "json.h"

namespace hegel {
    class TestCase;
}

/// @cond INTERNAL
namespace hegel::internal {
    hegel::internal::json::json
    communicate_with_core(const hegel::internal::json::json& schema,
                          const hegel::TestCase& tc);

    /* Exception thrown when a test case is rejected and should be
     * discarded (e.g. by `TestCase::assume(false)`, an exhausted
     * `filter()`, or an `UnsatisfiedAssumption` from the server).
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
     * `mark_complete` — the server already knows the iteration is over.
     */
    class HegelStopTest : public std::exception {
      public:
        const char* what() const noexcept override {
            return "test case stopped by backend";
        }
    };

} // namespace hegel::internal
/// @endcond
