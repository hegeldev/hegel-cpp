/**
 * @file grouping.h
 * @brief Span management and grouped generation for Hegel SDK
 *
 * Contains start_span(), stop_span(), group(), discardable_group(),
 * and the labels namespace with predefined span label constants.
 */

#ifndef HEGEL_GROUPING_HPP
#define HEGEL_GROUPING_HPP

#include <cstdint>

namespace hegel {

// =============================================================================
// Span Management
// =============================================================================

/**
 * @brief Start a labeled span for grouped generation.
 *
 * Low-level API. Prefer group() or discardable_group() instead.
 *
 * @param label Numeric label identifying the span type
 * @see labels namespace for predefined label constants
 */
void start_span(uint64_t label);

/**
 * @brief Stop the current span.
 *
 * Low-level API. Prefer group() or discardable_group() instead.
 *
 * @param discard If true, discard this span's generated data
 */
void stop_span(bool discard = false);

// =============================================================================
// Group Helpers
// =============================================================================

/**
 * @brief Run a function within a labeled group.
 *
 * Groups related generation calls together, which helps the testing
 * engine understand the structure of generated data and improve shrinking.
 *
 * @tparam F Function type
 * @param label Numeric label identifying the group type
 * @param f Function to execute within the group
 * @return The return value of f()
 */
template <typename F>
auto group(uint64_t label, F&& f) -> decltype(f()) {
  start_span(label);
  auto result = f();
  stop_span(false);
  return result;
}

/**
 * @brief Run a function within a discardable labeled group.
 *
 * Similar to group(), but if f() returns std::nullopt, the span's
 * generated data is discarded. Useful for filter-like operations.
 *
 * @tparam F Function type returning std::optional
 * @param label Numeric label identifying the group type
 * @param f Function to execute within the group
 * @return The return value of f()
 */
template <typename F>
auto discardable_group(uint64_t label, F&& f) -> decltype(f()) {
  start_span(label);
  auto result = f();
  stop_span(!result.has_value());
  return result;
}

// =============================================================================
// Label Constants
// =============================================================================

/**
 * @brief Predefined span labels for structured generation.
 *
 * These labels help the testing engine understand data structure
 * for better shrinking and reporting.
 */
namespace labels {
constexpr uint64_t LIST = 1;          ///< List/vector container
constexpr uint64_t LIST_ELEMENT = 2;  ///< Element within a list
constexpr uint64_t SET = 3;           ///< Set container
constexpr uint64_t SET_ELEMENT = 4;   ///< Element within a set
constexpr uint64_t MAP = 5;           ///< Map/dictionary container
constexpr uint64_t MAP_ENTRY = 6;     ///< Key-value entry in a map
constexpr uint64_t TUPLE = 7;         ///< Tuple container
constexpr uint64_t ONE_OF = 8;        ///< Choice from multiple generators
constexpr uint64_t OPTIONAL = 9;      ///< Optional value
constexpr uint64_t FIXED_DICT = 10;   ///< Fixed-key dictionary (struct)
constexpr uint64_t FLAT_MAP = 11;     ///< Dependent generation
constexpr uint64_t FILTER = 12;       ///< Filtered generation
}  // namespace labels

}  // namespace hegel

#endif  // HEGEL_GROUPING_HPP
