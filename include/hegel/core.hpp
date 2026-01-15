/**
 * @file core.hpp
 * @brief Core types and functions for Hegel SDK
 *
 * Contains Mode enum, HegelOptions, HegelReject exception, and
 * fundamental functions like assume() and note().
 */

#ifndef HEGEL_CORE_HPP
#define HEGEL_CORE_HPP

#include <optional>
#include <stdexcept>
#include <string>

namespace hegel {

// =============================================================================
// Constants
// =============================================================================

inline constexpr int ASSERTION_FAILURE_EXIT_CODE = 134;

// =============================================================================
// Mode and State
// =============================================================================

/**
 * @brief Execution mode for the Hegel SDK.
 *
 * Determines how the SDK communicates with the Hegel test runner.
 */
enum class Mode {
  /**
   * @brief Standalone mode - test binary manages its own lifecycle.
   *
   * In this mode, the Hegel CLI is running externally and connects via
   * a Unix socket specified by the HEGEL_SOCKET environment variable.
   * The test binary is invoked repeatedly by Hegel.
   */
  Standalone,
  /**
   * @brief Embedded mode - test binary spawns Hegel as a subprocess.
   *
   * In this mode, the test binary uses hegel() to run property tests.
   * The binary spawns Hegel as a subprocess and communicates over a
   * Unix socket it creates. This is the recommended mode for most use cases.
   */
  Embedded
};

/**
 * @brief Get the current execution mode.
 * @return The current Mode (Standalone or Embedded)
 */
Mode current_mode();

/**
 * @brief Check if this is the last test run.
 *
 * Useful in embedded mode to only print output once, after shrinking
 * is complete and the final failing case has been found.
 *
 * @return true if this is the last (possibly failing) run
 */
bool is_last_run();

/**
 * @brief Print a note message for debugging.
 *
 * In standalone mode, always prints to stderr.
 * In embedded mode, only prints on the last run to avoid cluttering
 * output during the many test iterations.
 *
 * @param message The message to print
 */
void note(const std::string& message);

// =============================================================================
// Embedded Mode Options
// =============================================================================

/**
 * @brief Configuration options for embedded mode execution.
 * @see hegel()
 */
struct HegelOptions {
  /// Number of test cases to run. Default: 100
  std::optional<uint64_t> test_cases;
  /// Enable debug output from hegel subprocess
  bool debug = false;
  /// Path to the hegel binary. Default: "hegel" (uses PATH)
  std::optional<std::string> hegel_path;
};

// =============================================================================
// Embedded Mode Exception
// =============================================================================

/**
 * @brief Exception thrown when a test case is rejected.
 *
 * Thrown by assume(false) in embedded mode to signal that the current
 * test case should be discarded rather than counted as a failure.
 * This is caught by the hegel() function's test runner.
 */
class HegelReject : public std::exception {
 public:
  const char* what() const noexcept override { return "assume failed"; }
};

// =============================================================================
// Core API Functions
// =============================================================================

/**
 * @brief Stop the current test case immediately.
 *
 * In embedded mode, throws HegelReject which is caught by hegel().
 * In standalone mode, exits with the HEGEL_REJECT_CODE exit code.
 *
 * @note This function never returns.
 */
[[noreturn]] void stop_test();

/**
 * @brief Assume a condition is true; reject if false.
 *
 * Use this when generated data doesn't meet test preconditions.
 * This signals to Hegel that the input is invalid and should be
 * discarded, not counted as a test failure.
 *
 * @code{.cpp}
 * auto person = person_gen.generate();
 * hegel::assume(person.age >= 18);  // Only test adults
 * // ... rest of test
 * @endcode
 *
 * @param condition The condition to check
 */
void assume(bool condition);

}  // namespace hegel

#endif  // HEGEL_CORE_HPP
