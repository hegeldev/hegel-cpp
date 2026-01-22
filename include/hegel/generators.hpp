/**
 * @file generators.hpp
 * @brief Generator and DefaultGenerator class templates for Hegel SDK
 *
 * Contains the core Generator<T> and DefaultGenerator<T> class templates,
 * the Response wrapper, and factory functions.
 */

#ifndef HEGEL_GENERATORS_HPP
#define HEGEL_GENERATORS_HPP

#include <functional>
#include <optional>
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <string>
#include <type_traits>

#include "core.hpp"
#include "detail.hpp"

namespace hegel {

// =============================================================================
// Forward declarations
// =============================================================================

template <typename T>
class Generator;
template <typename T>
class DefaultGenerator;

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
 * auto gen =
 * hegel::from_schema<int>(R"({"type":"integer","minimum":0,"maximum":100})"); int value
 * = gen.generate();
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

}  // namespace hegel

#endif  // HEGEL_GENERATORS_HPP
