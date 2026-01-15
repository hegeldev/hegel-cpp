/**
 * @file hegel.hpp
 * @brief Hegel C++ SDK - Hypothesis-like property-based testing for C++20
 *
 * This library provides type-safe random data generation for property-based
 * testing, communicating with the Hegel test runner via Unix sockets.
 *
 * @section usage Basic Usage
 *
 * @code{.cpp}
 * #include "hegel/hegel.hpp"
 *
 * // Type-based generation (schema derived via reflect-cpp)
 * auto gen = hegel::default_generator<Person>();
 * Person p = gen.generate();
 *
 * // Strategy-based generation
 * using namespace hegel::st;
 * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
 * int value = int_gen.generate();
 * @endcode
 *
 * @section env Environment Variables
 * - `HEGEL_SOCKET`: Path to the Unix socket for generation requests
 * - `HEGEL_REJECT_CODE`: Exit code to use when assume(false) is called
 * - `HEGEL_DEBUG`: If set, prints REQUEST/RESPONSE JSON to stderr
 *
 * @section deps Dependencies
 * - reflect-cpp (https://github.com/getml/reflect-cpp) - C++20 reflection
 * - nlohmann/json (https://github.com/nlohmann/json) - JSON manipulation
 * - POSIX sockets (sys/socket.h, sys/un.h, unistd.h)
 */

#ifndef HEGEL_HPP
#define HEGEL_HPP

#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// POSIX headers
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// C++ filesystem
#include <filesystem>

// reflect-cpp headers
#include <rfl.hpp>
#include <rfl/json.hpp>

// nlohmann/json for schema manipulation
#include <nlohmann/json.hpp>

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

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
// Forward declarations
// =============================================================================

template <typename T>
class Generator;
template <typename T>
class DefaultGenerator;

// =============================================================================
// Non-template function declarations (implemented in hegel.cpp)
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

// =============================================================================
// Internal detail namespace
// =============================================================================

namespace detail {

int get_reject_code();
std::string get_socket_path();

// Connection lifecycle management
bool is_connected();
void open_connection();
void close_connection();
size_t get_span_depth();
void increment_span_depth();
void decrement_span_depth();
nlohmann::json send_request(const std::string& command, const nlohmann::json& payload);
std::string communicate_with_socket(const std::string& schema);

}  // namespace detail

// =============================================================================
// Grouped Generation
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

// =============================================================================
// Response wrapper for socket communication
// =============================================================================

/// @cond INTERNAL
template <typename T>
struct Response {
  std::optional<T> result;
  std::optional<std::string> error;
};
/// @endcond

// =============================================================================
// Generator class template
// =============================================================================

/**
 * @brief A generator that produces random values of type T.
 *
 * Generator is the core abstraction for random data generation. It wraps
 * a function that produces values and provides combinators for transforming
 * and composing generators.
 *
 * @code{.cpp}
 * using namespace hegel::st;
 *
 * // Create a generator
 * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
 * int value = int_gen.generate();
 *
 * // Transform with map
 * auto squared = int_gen.map([](int x) { return x * x; });
 *
 * // Filter values
 * auto even = int_gen.filter([](int x) { return x % 2 == 0; });
 *
 * // Dependent generation with flatmap
 * auto sized = integers<size_t>({.min_value = 1, .max_value = 10})
 *     .flatmap([](size_t len) {
 *         return text({.min_size = len, .max_size = len});
 *     });
 * @endcode
 *
 * @tparam T The type of values this generator produces
 */
template <typename T>
class Generator {
 private:
  std::function<T()> gen_fn_;
  std::optional<std::string> schema_;

 public:
  /**
   * @brief Construct a generator from a function.
   * @param fn Function that produces values of type T
   */
  explicit Generator(std::function<T()> fn) : gen_fn_(std::move(fn)) {}

  /**
   * @brief Construct a generator with an associated JSON schema.
   * @param fn Function that produces values of type T
   * @param schema JSON schema string for this generator
   */
  Generator(std::function<T()> fn, std::string schema)
      : gen_fn_(std::move(fn)), schema_(std::move(schema)) {}

  /**
   * @brief Get the JSON schema for this generator, if any.
   * @return Optional containing the schema, or nullopt if not schema-based
   */
  const std::optional<std::string>& schema() const { return schema_; }

  /**
   * @brief Generate a random value.
   * @return A randomly generated value of type T
   */
  T generate() const { return gen_fn_(); }

  /**
   * @brief Transform generated values with a function.
   *
   * Creates a new generator that applies a transformation to each value.
   *
   * @code{.cpp}
   * auto squared = integers<int>().map([](int x) { return x * x; });
   * @endcode
   *
   * @tparam F Function type (T -> result type)
   * @param f Transformation function
   * @return Generator producing transformed values
   */
  template <typename F>
  auto map(F&& f) const -> Generator<std::invoke_result_t<F, T>> {
    using U = std::invoke_result_t<F, T>;
    auto inner = gen_fn_;
    return Generator<U>([inner, f = std::forward<F>(f)]() -> U { return f(inner()); });
  }

  /**
   * @brief Chain generators for dependent generation.
   *
   * Creates a new generator where the second generator depends on the
   * value from the first. Useful when generation parameters depend on
   * previously generated values.
   *
   * @code{.cpp}
   * auto sized_string = integers<size_t>({.min_value = 1, .max_value = 10})
   *     .flatmap([](size_t len) {
   *         return text({.min_size = len, .max_size = len});
   *     });
   * @endcode
   *
   * @tparam F Function type (T -> Generator)
   * @param f Function that takes a T and returns a Generator
   * @return Generator producing values from the chained generator
   */
  template <typename F>
  auto flatmap(F&& f) const -> std::invoke_result_t<F, T> {
    using ResultGen = std::invoke_result_t<F, T>;
    using U = decltype(std::declval<ResultGen>().generate());
    auto inner = gen_fn_;
    return Generator<U>(
        [inner, f = std::forward<F>(f)]() -> U { return f(inner()).generate(); });
  }

  /**
   * @brief Filter generated values by a predicate.
   *
   * Creates a generator that only produces values satisfying the predicate.
   * If max_attempts consecutive values fail the predicate, calls assume(false)
   * to reject the test case.
   *
   * @code{.cpp}
   * auto even = integers<int>({.min_value = 0, .max_value = 100})
   *     .filter([](int x) { return x % 2 == 0; }, 10);
   * @endcode
   *
   * @param pred Predicate that values must satisfy
   * @param max_attempts Maximum generation attempts before rejecting
   * @return Generator<T> producing only values satisfying pred
   */
  Generator<T> filter(std::function<bool(const T&)> pred, int max_attempts = 3) const {
    auto inner = gen_fn_;
    return Generator<T>([inner, pred, max_attempts]() -> T {
      for (int i = 0; i < max_attempts; ++i) {
        T value = inner();
        if (pred(value)) {
          return value;
        }
      }
      assume(false);
      std::abort();  // Unreachable, but needed for return type
    });
  }
};

// =============================================================================
// DefaultGenerator class template
// =============================================================================

/**
 * @brief A generator that derives its schema from type T using reflect-cpp.
 *
 * DefaultGenerator automatically creates a JSON schema from the structure
 * of type T using reflect-cpp's compile-time reflection. This allows
 * generating random instances of structs and classes without manually
 * specifying the schema.
 *
 * @code{.cpp}
 * struct Person {
 *     std::string name;
 *     int age;
 * };
 *
 * auto gen = hegel::default_generator<Person>();
 * Person p = gen.generate();
 * @endcode
 *
 * Supports all the same combinators as Generator (map, flatmap, filter)
 * by converting to a Generator internally.
 *
 * @tparam T The type to generate values for (must be reflect-cpp compatible)
 * @see default_generator() Factory function for creating DefaultGenerators
 */
template <typename T>
class DefaultGenerator {
 private:
  std::string schema_;

  T generate_impl() const {
    std::string response_json = detail::communicate_with_socket(schema_);

    auto parse_result = rfl::json::read<Response<T>>(response_json);
    assume(parse_result.has_value());

    const Response<T>& response = parse_result.value();

    assume(!response.error);
    assume(response.result.has_value());

    return *response.result;
  }

 public:
  /// Default constructor - derives schema from T using reflect-cpp
  DefaultGenerator() : schema_(rfl::json::to_schema<T>()) {}

  /// Get mutable reference to the JSON schema
  std::string& schema() { return schema_; }
  /// Get const reference to the JSON schema
  const std::string& schema() const { return schema_; }

  /**
   * @brief Generate a random value of type T.
   * @return A randomly generated value
   */
  T generate() const { return generate_impl(); }

  /**
   * @brief Convert to a Generator<T>.
   *
   * Allows using DefaultGenerator where Generator is expected.
   *
   * @return A Generator<T> with the same schema
   */
  Generator<T> to_generator() const {
    std::string schema_copy = schema_;
    return Generator<T>([schema_copy]() -> T {
      std::string response_json = detail::communicate_with_socket(schema_copy);

      auto parse_result = rfl::json::read<Response<T>>(response_json);
      assume(parse_result.has_value());

      const Response<T>& response = parse_result.value();

      assume(!response.error);
      assume(response.result.has_value());

      return *response.result;
    });
  }

  /// Implicit conversion to Generator<T>
  operator Generator<T>() const { return to_generator(); }

  /// Transform generated values with a function
  /// @see Generator::map
  template <typename F>
  auto map(F&& f) const -> Generator<std::invoke_result_t<F, T>> {
    return to_generator().map(std::forward<F>(f));
  }

  /// Chain generators for dependent generation
  /// @see Generator::flatmap
  template <typename F>
  auto flatmap(F&& f) const -> std::invoke_result_t<F, T> {
    return to_generator().flatmap(std::forward<F>(f));
  }

  /// Filter generated values by a predicate
  /// @see Generator::filter
  Generator<T> filter(std::function<bool(const T&)> pred) const {
    return to_generator().filter(std::move(pred));
  }
};

// =============================================================================
// Factory functions
// =============================================================================

/**
 * @brief Create a generator for type T using automatic schema derivation.
 *
 * Uses reflect-cpp to derive a JSON schema from the type's structure.
 * Works with structs, classes, and standard library types.
 *
 * @code{.cpp}
 * struct Point { double x; double y; };
 * auto gen = hegel::default_generator<Point>();
 * Point p = gen.generate();
 * @endcode
 *
 * @tparam T The type to generate (must be reflect-cpp compatible)
 * @return A DefaultGenerator<T> instance
 */
template <typename T>
DefaultGenerator<T> default_generator() {
  return DefaultGenerator<T>();
}

/**
 * @brief Create a generator from an explicit JSON schema.
 *
 * Use this when you need fine-grained control over the generation
 * schema, or when working with types that don't support automatic
 * schema derivation.
 *
 * @code{.cpp}
 * auto gen = hegel::from_schema<int>(R"({"type":"integer","minimum":0,"maximum":100})");
 * int value = gen.generate();
 * @endcode
 *
 * @tparam T The type to deserialize generated values into
 * @param schema JSON schema string describing the generation constraints
 * @return A Generator<T> that generates according to the schema
 */
template <typename T>
Generator<T> from_schema(std::string schema) {
  return Generator<T>(
      [schema]() -> T {
        std::string response_json = detail::communicate_with_socket(schema);

        auto parse_result = rfl::json::read<Response<T>>(response_json);
        assume(parse_result.has_value());

        const Response<T>& response = parse_result.value();

        assume(!response.error);
        assume(response.result.has_value());

        return *response.result;
      },
      schema);
}

// =============================================================================
// Strategies namespace
// =============================================================================

/**
 * @brief Namespace containing all strategy functions for data generation.
 *
 * Strategies are factory functions that create Generator instances with
 * specific generation behaviors. Import this namespace to use the
 * strategy functions directly.
 *
 * @code{.cpp}
 * using namespace hegel::st;
 *
 * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
 * auto str_gen = text({.min_size = 1, .max_size = 50});
 * auto bool_gen = booleans();
 * @endcode
 */
namespace st {

// =============================================================================
// Parameter structs
// =============================================================================

/**
 * @brief Parameters for integers() strategy.
 * @tparam T The integer type
 */
template <typename T>
struct IntegersParams {
  std::optional<T> min_value;  ///< Minimum value (inclusive). Default: type minimum
  std::optional<T> max_value;  ///< Maximum value (inclusive). Default: type maximum
};

/**
 * @brief Parameters for floats() strategy.
 * @tparam T The floating point type
 */
template <typename T>
struct FloatsParams {
  std::optional<T> min_value;  ///< Minimum value. Default: no minimum
  std::optional<T> max_value;  ///< Maximum value. Default: no maximum
  bool exclude_min = false;    ///< If true, exclude min_value (exclusive bound)
  bool exclude_max = false;    ///< If true, exclude max_value (exclusive bound)
};

/**
 * @brief Parameters for text() strategy.
 */
struct TextParams {
  size_t min_size = 0;               ///< Minimum string length
  std::optional<size_t> max_size;    ///< Maximum string length. Default: no limit
};

/**
 * @brief Parameters for domains() strategy.
 */
struct DomainsParams {
  size_t max_length = 255;  ///< Maximum domain name length
};

/**
 * @brief Parameters for ip_addresses() strategy.
 */
struct IpAddressesParams {
  std::optional<int> v;  ///< IP version: 4, 6, or nullopt for both
};

/**
 * @brief Parameters for vectors() strategy.
 */
struct VectorsParams {
  size_t min_size = 0;               ///< Minimum vector size
  std::optional<size_t> max_size;    ///< Maximum vector size. Default: 100
  bool unique = false;               ///< If true, all elements must be unique
};

/**
 * @brief Parameters for sets() strategy.
 */
struct SetsParams {
  size_t min_size = 0;               ///< Minimum set size
  std::optional<size_t> max_size;    ///< Maximum set size. Default: 20
};

/**
 * @brief Parameters for dictionaries() strategy.
 */
struct DictionariesParams {
  size_t min_size = 0;               ///< Minimum number of entries
  std::optional<size_t> max_size;    ///< Maximum number of entries. Default: 20
};

// =============================================================================
// Non-template strategy declarations (implemented in hegel.cpp)
// =============================================================================

/// @name Primitive Strategies
/// @{

/// Generate null values (std::monostate)
Generator<std::monostate> nulls();

/// Generate random boolean values
Generator<bool> booleans();

/// @}

/// @name String Strategies
/// @{

/**
 * @brief Generate random text strings.
 * @param params Length constraints
 * @return Generator producing random strings
 */
Generator<std::string> text(TextParams params = {});

/**
 * @brief Generate strings matching a regular expression.
 * @param pattern Regex pattern to match
 * @return Generator producing matching strings
 */
Generator<std::string> from_regex(const std::string& pattern);

/// Generate valid email addresses
Generator<std::string> emails();

/**
 * @brief Generate valid domain names.
 * @param params Length constraints
 * @return Generator producing domain names
 */
Generator<std::string> domains(DomainsParams params = {});

/// Generate valid URLs
Generator<std::string> urls();

/**
 * @brief Generate IP addresses.
 * @param params Version constraints (v=4, v=6, or both)
 * @return Generator producing IP address strings
 */
Generator<std::string> ip_addresses(IpAddressesParams params = {});

/// Generate dates in ISO 8601 format (YYYY-MM-DD)
Generator<std::string> dates();

/// Generate times in ISO 8601 format (HH:MM:SS)
Generator<std::string> times();

/// Generate datetimes in ISO 8601 format
Generator<std::string> datetimes();

/// @}

// =============================================================================
// Template strategies (must be in header)
// =============================================================================

/// @name Constant and Numeric Strategies
/// @{

/**
 * @brief Generate a constant value.
 *
 * Always returns the same value. Useful for creating fixed elements
 * in composite generators.
 *
 * @code{.cpp}
 * auto answer = just(42);
 * auto greeting = just("hello");
 * @endcode
 *
 * @tparam T The value type
 * @param value The constant value to generate
 * @return Generator that always produces value
 */
template <typename T>
Generator<T> just(T value) {
  nlohmann::json schema;

  if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::nullptr_t> ||
                std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
    schema = {{"const", value}};
    return Generator<T>([value]() { return value; }, schema.dump());
  } else {
    return Generator<T>([value]() { return value; });
  }
}

/// @overload just(const char*) - convenience overload for string literals
inline Generator<std::string> just(const char* value) {
  return just(std::string(value));
}

/**
 * @brief Generate random integers.
 *
 * @code{.cpp}
 * auto any_int = integers<int>();
 * auto bounded = integers<int>({.min_value = 0, .max_value = 100});
 * auto positive = integers<int>({.min_value = 1});
 * @endcode
 *
 * @tparam T Integer type (default: int64_t)
 * @param params Bounds constraints
 * @return Generator producing integers in the specified range
 */
template <typename T = int64_t>
  requires std::is_integral_v<T>
Generator<T> integers(IntegersParams<T> params = {}) {
  nlohmann::json schema = {{"type", "integer"}};

  T min_val = params.min_value.value_or(std::numeric_limits<T>::min());
  T max_val = params.max_value.value_or(std::numeric_limits<T>::max());

  schema["minimum"] = min_val;
  schema["maximum"] = max_val;

  return from_schema<T>(schema.dump());
}

/**
 * @brief Generate random floating point numbers.
 *
 * @code{.cpp}
 * auto any_float = floats<double>();
 * auto unit = floats<double>({.min_value = 0.0, .max_value = 1.0});
 * auto open = floats<double>({
 *     .min_value = 0.0, .max_value = 1.0,
 *     .exclude_min = true, .exclude_max = true
 * });
 * @endcode
 *
 * @tparam T Floating point type (default: double)
 * @param params Bounds and exclusion constraints
 * @return Generator producing floats in the specified range
 */
template <typename T = double>
  requires std::is_floating_point_v<T>
Generator<T> floats(FloatsParams<T> params = {}) {
  nlohmann::json schema = {{"type", "number"}};

  if (params.min_value) {
    if (params.exclude_min) {
      schema["exclusiveMinimum"] = *params.min_value;
    } else {
      schema["minimum"] = *params.min_value;
    }
  }

  if (params.max_value) {
    if (params.exclude_max) {
      schema["exclusiveMaximum"] = *params.max_value;
    } else {
      schema["maximum"] = *params.max_value;
    }
  }

  return from_schema<T>(schema.dump());
}

/// @}

/// @name Collection Strategies
/// @{

/**
 * @brief Generate vectors with elements from another generator.
 *
 * @code{.cpp}
 * auto int_vec = vectors(integers<int>());
 * auto bounded = vectors(integers<int>(), {.min_size = 1, .max_size = 10});
 * auto unique_vec = vectors(integers<int>(), {.unique = true});
 * @endcode
 *
 * @tparam T Element type
 * @param elements Generator for vector elements
 * @param params Size and uniqueness constraints
 * @return Generator producing vectors
 */
template <typename T>
Generator<std::vector<T>> vectors(Generator<T> elements, VectorsParams params = {}) {
  if (elements.schema()) {
    nlohmann::json elem_schema = nlohmann::json::parse(*elements.schema());
    nlohmann::json schema = {{"type", "array"}, {"items", elem_schema}};

    if (params.min_size > 0) schema["minItems"] = params.min_size;
    if (params.max_size) schema["maxItems"] = *params.max_size;
    if (params.unique) schema["uniqueItems"] = true;

    return from_schema<std::vector<T>>(schema.dump());
  }

  size_t max_size = params.max_size.value_or(100);

  auto length_gen =
      integers<size_t>({.min_value = params.min_size, .max_value = max_size});

  return Generator<std::vector<T>>([elements, length_gen]() {
    size_t len = length_gen.generate();
    std::vector<T> result;
    result.reserve(len);

    for (size_t i = 0; i < len; ++i) {
      result.push_back(elements.generate());
    }

    return result;
  });
}

/**
 * @brief Generate sets with elements from another generator.
 *
 * @code{.cpp}
 * auto int_set = sets(integers<int>());
 * auto bounded = sets(integers<int>(), {.min_size = 1, .max_size = 5});
 * @endcode
 *
 * @tparam T Element type (must be comparable)
 * @param elements Generator for set elements
 * @param params Size constraints
 * @return Generator producing sets
 */
template <typename T>
Generator<std::set<T>> sets(Generator<T> elements, SetsParams params = {}) {
  if (elements.schema()) {
    nlohmann::json elem_schema = nlohmann::json::parse(*elements.schema());
    nlohmann::json schema = {
        {"type", "array"}, {"items", elem_schema}, {"uniqueItems", true}};

    if (params.min_size > 0) schema["minItems"] = params.min_size;
    if (params.max_size) schema["maxItems"] = *params.max_size;

    auto vec_gen = from_schema<std::vector<T>>(schema.dump());

    return Generator<std::set<T>>([vec_gen]() {
      auto vec = vec_gen.generate();
      return std::set<T>(vec.begin(), vec.end());
    });
  }

  size_t max_size = params.max_size.value_or(20);
  auto length_gen =
      integers<size_t>({.min_value = params.min_size, .max_value = max_size});

  return Generator<std::set<T>>([elements, length_gen]() {
    size_t target_len = length_gen.generate();
    std::set<T> result;

    size_t attempts = 0;
    while (result.size() < target_len && attempts < target_len * 10) {
      result.insert(elements.generate());
      ++attempts;
    }

    return result;
  });
}

/**
 * @brief Generate dictionaries (maps with string keys).
 *
 * @note Keys must be strings due to JSON schema limitations.
 *
 * @code{.cpp}
 * auto dict = dictionaries(text(), integers<int>());
 * auto bounded = dictionaries(text(), integers<int>(), {.min_size = 1, .max_size = 3});
 * @endcode
 *
 * @tparam K Key type (must be std::string)
 * @tparam V Value type
 * @param keys Generator for map keys
 * @param values Generator for map values
 * @param params Size constraints
 * @return Generator producing maps
 */
template <typename K, typename V>
  requires std::is_same_v<K, std::string>
Generator<std::map<K, V>> dictionaries(Generator<K> keys, Generator<V> values,
                                       DictionariesParams params = {}) {
  if (values.schema()) {
    nlohmann::json schema = {
        {"type", "object"},
        {"additionalProperties", nlohmann::json::parse(*values.schema())}};

    if (params.min_size > 0) schema["minProperties"] = params.min_size;
    if (params.max_size) schema["maxProperties"] = *params.max_size;

    return from_schema<std::map<K, V>>(schema.dump());
  }

  size_t max_size = params.max_size.value_or(20);
  auto length_gen =
      integers<size_t>({.min_value = params.min_size, .max_value = max_size});

  return Generator<std::map<K, V>>([keys, values, length_gen]() {
    size_t len = length_gen.generate();
    std::map<K, V> result;

    while (result.size() < len) {
      K key = keys.generate();
      V value = values.generate();
      result[std::move(key)] = std::move(value);
    }

    return result;
  });
}

/// @cond INTERNAL
namespace detail {

template <typename... Gens>
auto make_tuple_schema(const Gens&... gens) -> std::optional<std::string> {
  bool all_have_schemas = (gens.schema().has_value() && ...);
  if (!all_have_schemas) return std::nullopt;

  nlohmann::json prefix_items = nlohmann::json::array();
  (prefix_items.push_back(nlohmann::json::parse(*gens.schema())), ...);

  nlohmann::json schema = {{"type", "array"},
                           {"prefixItems", prefix_items},
                           {"items", false},
                           {"minItems", sizeof...(Gens)},
                           {"maxItems", sizeof...(Gens)}};

  return schema.dump();
}

template <typename Tuple, typename GenTuple, size_t... Is>
Tuple generate_tuple_impl(const GenTuple& gens, std::index_sequence<Is...>) {
  return Tuple{std::get<Is>(gens).generate()...};
}

}  // namespace detail
/// @endcond

/**
 * @brief Generate tuples from multiple generators.
 *
 * Each generator produces one element of the resulting tuple.
 *
 * @code{.cpp}
 * auto pair = tuples(integers<int>(), text());
 * auto triple = tuples(booleans(), integers<int>(), floats<double>());
 * @endcode
 *
 * @tparam Ts Element types
 * @param gens Generators for each tuple element
 * @return Generator producing tuples
 */
template <typename... Ts>
Generator<std::tuple<Ts...>> tuples(Generator<Ts>... gens) {
  using ResultTuple = std::tuple<Ts...>;

  auto maybe_schema = detail::make_tuple_schema(gens...);

  if (maybe_schema) {
    return from_schema<ResultTuple>(*maybe_schema);
  }

  auto gen_tuple = std::make_tuple(std::move(gens)...);

  return Generator<ResultTuple>([gen_tuple]() {
    return detail::generate_tuple_impl<ResultTuple>(gen_tuple,
                                                    std::index_sequence_for<Ts...>{});
  });
}

/// @}

/// @name Combinator Strategies
/// @{

/**
 * @brief Sample uniformly from a fixed set of values.
 *
 * @code{.cpp}
 * auto color = sampled_from({"red", "green", "blue"});
 * auto digit = sampled_from({1, 2, 3, 4, 5});
 * @endcode
 *
 * @tparam T Element type
 * @param elements Vector of values to sample from (must not be empty)
 * @return Generator that picks uniformly from elements
 */
template <typename T>
Generator<T> sampled_from(std::vector<T> elements) {
  assume(!elements.empty());

  if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::nullptr_t> ||
                std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
    nlohmann::json schema = {{"enum", elements}};
    return from_schema<T>(schema.dump());
  } else {
    auto index_gen =
        integers<size_t>({.min_value = 0, .max_value = elements.size() - 1});

    return Generator<T>([elements, index_gen]() {
      size_t idx = index_gen.generate();
      return elements[idx];
    });
  }
}

/// @overload sampled_from(std::initializer_list<T>)
template <typename T>
Generator<T> sampled_from(std::initializer_list<T> elements) {
  return sampled_from(std::vector<T>(elements));
}

/// @overload
inline Generator<std::string> sampled_from(
    std::initializer_list<const char*> elements) {
  std::vector<std::string> strings;
  strings.reserve(elements.size());
  for (const char* s : elements) {
    strings.push_back(s);
  }
  return sampled_from(std::move(strings));
}

/// @cond INTERNAL
namespace detail {

template <typename T>
auto make_one_of_schema(const std::vector<Generator<T>>& gens)
    -> std::optional<std::string> {
  for (const auto& gen : gens) {
    if (!gen.schema()) return std::nullopt;
  }

  nlohmann::json any_of = nlohmann::json::array();
  for (const auto& gen : gens) {
    any_of.push_back(nlohmann::json::parse(*gen.schema()));
  }

  nlohmann::json schema = {{"anyOf", any_of}};
  return schema.dump();
}

}  // namespace detail
/// @endcond

/**
 * @brief Choose from multiple generators of the same type.
 *
 * Randomly selects one generator and uses it to produce a value.
 *
 * @code{.cpp}
 * auto range = one_of({
 *     integers<int>({.min_value = 0, .max_value = 10}),
 *     integers<int>({.min_value = 100, .max_value = 110})
 * });
 * @endcode
 *
 * @tparam T Value type (all generators must produce this type)
 * @param gens Vector of generators to choose from (must not be empty)
 * @return Generator that delegates to a randomly chosen generator
 */
template <typename T>
Generator<T> one_of(std::vector<Generator<T>> gens) {
  assume(!gens.empty());

  auto maybe_schema = detail::make_one_of_schema(gens);

  if (maybe_schema) {
    return from_schema<T>(*maybe_schema);
  }

  auto index_gen = integers<size_t>({.min_value = 0, .max_value = gens.size() - 1});

  return Generator<T>([gens, index_gen]() {
    size_t idx = index_gen.generate();
    return gens[idx].generate();
  });
}

/// @overload one_of(std::initializer_list<Generator<T>>)
template <typename T>
Generator<T> one_of(std::initializer_list<Generator<T>> gens) {
  return one_of(std::vector<Generator<T>>(gens));
}

/// @cond INTERNAL
namespace detail {

template <typename Variant, typename GenTuple, size_t I = 0>
Variant generate_variant_impl(const GenTuple& gens, size_t idx) {
  if constexpr (I < std::tuple_size_v<GenTuple>) {
    if (idx == I) {
      return std::get<I>(gens).generate();
    }
    return generate_variant_impl<Variant, GenTuple, I + 1>(gens, idx);
  } else {
    return Variant{};
  }
}

}  // namespace detail
/// @endcond

/**
 * @brief Generate std::variant from heterogeneous generators.
 *
 * Each generator produces one possible variant alternative.
 *
 * @code{.cpp}
 * auto value = variant_(integers<int>(), text(), booleans());
 * // Returns std::variant<int, std::string, bool>
 * @endcode
 *
 * @tparam Ts Variant alternative types
 * @param gens Generators for each alternative
 * @return Generator producing variants
 */
template <typename... Ts>
Generator<std::variant<Ts...>> variant_(Generator<Ts>... gens) {
  using ResultVariant = std::variant<Ts...>;
  constexpr size_t N = sizeof...(Ts);

  auto gen_tuple = std::make_tuple(std::move(gens)...);
  auto index_gen = integers<size_t>({.min_value = 0, .max_value = N - 1});

  return Generator<ResultVariant>([gen_tuple, index_gen]() {
    size_t idx = index_gen.generate();
    return detail::generate_variant_impl<ResultVariant, decltype(gen_tuple)>(gen_tuple,
                                                                             idx);
  });
}

/**
 * @brief Generate optional values (present or absent).
 *
 * Randomly produces either a value from the given generator or std::nullopt.
 *
 * @code{.cpp}
 * auto maybe_int = optional_(integers<int>());
 * // Returns std::optional<int>, may be nullopt
 * @endcode
 *
 * @tparam T Value type
 * @param gen Generator for the value when present
 * @return Generator producing optional values
 */
template <typename T>
Generator<std::optional<T>> optional_(Generator<T> gen) {
  auto bool_gen = booleans();

  return Generator<std::optional<T>>([gen, bool_gen]() -> std::optional<T> {
    bool is_none = bool_gen.generate();
    if (is_none) {
      return std::nullopt;
    }
    return gen.generate();
  });
}

/// @}

/// @name Object Construction Strategies
/// @{

/**
 * @brief Construct objects using positional constructor arguments.
 *
 * Generates constructor arguments from the provided generators and
 * calls T's constructor.
 *
 * @code{.cpp}
 * struct Point {
 *     Point(double x, double y) : x(x), y(y) {}
 *     double x, y;
 * };
 *
 * auto point = builds<Point>(
 *     floats<double>({.min_value = 0, .max_value = 100}),
 *     floats<double>({.min_value = 0, .max_value = 100})
 * );
 * @endcode
 *
 * @tparam T Type to construct
 * @tparam Gens Generator types for constructor arguments
 * @param gens Generators providing constructor arguments
 * @return Generator producing T instances
 */
template <typename T, typename... Gens>
Generator<T> builds(Gens... gens) {
  auto gen_tuple = std::make_tuple(std::move(gens)...);

  return Generator<T>([gen_tuple]() {
    return std::apply([](auto&... g) { return T(g.generate()...); }, gen_tuple);
  });
}

/**
 * @brief Helper for named field initialization in builds_agg().
 *
 * @tparam MemberPtr Pointer-to-member for the field
 * @tparam Gen Generator type for the field value
 */
template <auto MemberPtr, typename Gen>
struct Field {
  static constexpr auto member_ptr = MemberPtr;  ///< The member pointer
  Gen generator;                                  ///< Generator for the field value
};

/**
 * @brief Create a field specification for builds_agg().
 *
 * @code{.cpp}
 * auto rect = builds_agg<Rectangle>(
 *     field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value = 100})),
 *     field<&Rectangle::height>(integers<int>({.min_value = 1, .max_value = 100}))
 * );
 * @endcode
 *
 * @tparam MemberPtr Pointer-to-member specifying which field
 * @tparam Gen Generator type
 * @param gen Generator for the field value
 * @return A Field specification for use with builds_agg()
 */
template <auto MemberPtr, typename Gen>
Field<MemberPtr, Gen> field(Gen gen) {
  return Field<MemberPtr, Gen>{std::move(gen)};
}

/**
 * @brief Construct aggregates using named field initialization.
 *
 * Useful for structs where you want to specify fields by name rather
 * than position. Each field() specifies a member pointer and generator.
 *
 * @code{.cpp}
 * struct Rectangle {
 *     int width;
 *     int height;
 * };
 *
 * auto rect = builds_agg<Rectangle>(
 *     field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value = 100})),
 *     field<&Rectangle::height>(integers<int>({.min_value = 1, .max_value = 100}))
 * );
 * @endcode
 *
 * @tparam T Aggregate type to construct
 * @tparam Fields Field specification types
 * @param fields Field specifications from field()
 * @return Generator producing T instances
 */
template <typename T, typename... Fields>
Generator<T> builds_agg(Fields... fields) {
  auto gen_tuple = std::make_tuple(std::move(fields)...);

  return Generator<T>([gen_tuple]() mutable {
    T result{};
    std::apply(
        [&result](auto&... fs) {
          ((result.*(std::remove_reference_t<decltype(fs)>::member_ptr) =
                fs.generator.generate()),
           ...);
        },
        gen_tuple);
    return result;
  });
}

/// @}

}  // namespace st

// =============================================================================
// Embedded Mode Implementation
// =============================================================================

/// @cond INTERNAL
namespace detail {

// Thread-local mode state setters (used by embedded mode)
void set_mode(Mode m);
void set_is_last_run(bool v);
void set_embedded_connection(int fd);
void clear_embedded_connection();
std::string read_line(int fd);
void write_line(int fd, const std::string& line);

}  // namespace detail
/// @endcond

/**
 * @brief Run property-based tests using Hegel in embedded mode.
 *
 * This is the recommended way to run property tests. The function:
 * 1. Creates a Unix socket server for communication
 * 2. Spawns the Hegel CLI as a subprocess
 * 3. Runs the test function for each generated test case
 * 4. Handles shrinking when failures occur
 * 5. Throws std::runtime_error if any test case fails
 *
 * @code{.cpp}
 * #include "hegel/hegel.hpp"
 *
 * int main() {
 *     hegel::hegel([]() {
 *         using namespace hegel::st;
 *         auto x = integers<int>({.min_value = 0, .max_value = 100}).generate();
 *         auto y = integers<int>({.min_value = 0, .max_value = 100}).generate();
 *
 *         // Property: x + y >= x (true for non-negative integers)
 *         if (x + y < x) {
 *             throw std::runtime_error("Addition underflow!");
 *         }
 *     }, {.test_cases = 1000});
 *
 *     return 0;
 * }
 * @endcode
 *
 * @tparam F Test function type (callable with no arguments)
 * @param test_fn The test function to run repeatedly
 * @param options Configuration options (test count, debug mode, hegel path)
 * @throws std::runtime_error if any test case fails
 * @see HegelOptions for configuration options
 */
template <typename F>
void hegel(F&& test_fn, HegelOptions options = {}) {
  // Create temp directory with socket
  std::string temp_dir = "/tmp/hegel_" + std::to_string(getpid());
  std::filesystem::create_directories(temp_dir);
  std::string socket_path = temp_dir + "/hegel.sock";

  // Create server socket
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::copy(socket_path.begin(), socket_path.end(), addr.sun_path);

  if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(server_fd);
    throw std::runtime_error("Failed to bind socket");
  }

  if (listen(server_fd, 5) < 0) {
    close(server_fd);
    throw std::runtime_error("Failed to listen on socket");
  }

  // Build hegel command
  std::string hegel_path = options.hegel_path.value_or(HEGEL_DEFAULT_PATH);
  std::vector<std::string> args = {hegel_path, "--client-mode", socket_path,
                                   "--no-tui"};
  if (options.test_cases) {
    args.push_back("--test-cases");
    args.push_back(std::to_string(*options.test_cases));
  }
  if (options.debug) {
    args.push_back("--debug");
  }

  // Fork and exec hegel
  pid_t pid = fork();
  if (pid < 0) {
    close(server_fd);
    throw std::runtime_error("Failed to fork");
  }

  if (pid == 0) {
    // Child: exec hegel
    std::vector<char*> argv;
    for (auto& a : args) {
      argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    std::exit(1);
  }

  // Parent: accept connections until hegel exits
  fd_set fds;
  struct timeval tv;

  while (true) {
    FD_ZERO(&fds);
    FD_SET(server_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms

    int ready = select(server_fd + 1, &fds, nullptr, nullptr, &tv);

    if (ready > 0) {
      int client_fd = accept(server_fd, nullptr, nullptr);
      if (client_fd >= 0) {
        // Handle connection
        try {
          // Read handshake
          std::string line = detail::read_line(client_fd);
          auto handshake = nlohmann::json::parse(line);
          bool is_last = handshake.value("is_last_run", false);

          // Set thread-local state
          detail::set_mode(Mode::Embedded);
          detail::set_is_last_run(is_last);
          detail::set_embedded_connection(client_fd);

          // Send handshake_ack
          detail::write_line(client_fd, R"({"type": "handshake_ack"})");

          // Run test
          std::string result_type = "pass";
          std::string error_message;
          try {
            test_fn();
          } catch (const HegelReject& e) {
            result_type = "reject";
            error_message = e.what();
          } catch (const std::exception& e) {
            result_type = "fail";
            error_message = e.what();
          } catch (...) {
            result_type = "fail";
            error_message = "Unknown exception";
          }

          // Clear embedded connection
          detail::clear_embedded_connection();

          // Send result
          nlohmann::json result = {{"type", "test_result"}, {"result", result_type}};
          if (!error_message.empty()) {
            result["message"] = error_message;
          }
          detail::write_line(client_fd, result.dump());

          detail::set_mode(Mode::Standalone);
        } catch (...) {
          detail::clear_embedded_connection();
          detail::set_mode(Mode::Standalone);
        }
        close(client_fd);
      }
    }

    // Check if hegel exited
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        close(server_fd);
        std::filesystem::remove_all(temp_dir);
        throw std::runtime_error("Hegel test failed (exit code " +
                                 std::to_string(WEXITSTATUS(status)) + ")");
      }
      break;
    }
  }

  // Cleanup
  close(server_fd);
  std::filesystem::remove_all(temp_dir);
}

}  // namespace hegel

#endif  // HEGEL_HPP
