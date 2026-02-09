#pragma once

/**
 * @file generators.h
 * @brief Generator and SchemaBackedGenerator class templates for Hegel SDK
 *
 * Contains the core Generator<T> and SchemaBackedGenerator<T> class templates,
 * the Response wrapper, and factory functions.
 */

#include <memory>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include "internal.h"

/**
 * @brief Namespace containing abstractions for data generation.
 *
 * You wouldn't typically use the *classes* in this namespace directly,
 * but rather use the default_generator() function and the strategies
 * in hegel::strategies.
 */
namespace hegel::generators {
/**
 * @brief The base interface that defines Generators.
 *
 * IGenerator's core method is generate(), which produces a single value of type
 * T.
 *
 * IGenerator also has a schema() method that returns the schema (if there is
 * one).
 *
 * It's rare that you would need to implement IGenerator yourself; you would
 * typically use pre-existing primitives from the library or compose them using
 * compositors from the library. Even the library only uses IGenerator
 * sparingly; most operations are exposed in Generator, a class that implements
 * IGenerator.
 *
 * @tparam T The type to generate values for (must be reflect-cpp compatible)
 * @see default_generator() Factory function for creating default type-based
 * Generators
 * @see hegel::strategies Built-in functions that %generate data for you
 * @see Generator Sub-class that implements IGenerator; the thing you typically
 * interact with
 */
template <typename T> struct IGenerator {
    IGenerator() {}
    virtual ~IGenerator() = default;

    /**
     * @brief Generate a random value.
     * @return A randomly generated value of type T
     */
    virtual T generate() const = 0;

    /**
     * @brief Get the JSON schema for this generator, if any.
     *
     * All IGenerators *may* have a schema, even if the schema isn't directly
     * used for generating; this functionality may be used for composing
     * generators, for example.
     *
     * @return Optional containing the schema, or nullopt if not schema-based
     */
    virtual const std::optional<std::string_view> schema() const = 0;
};

/**
 * @brief A Generator; this is the class that most methods return.
 *
 * Generator is the core abstraction for random data generation. It wraps
 * an IGenerator (which produces values) and provides combinators  (map(),
 * flatmap(), filter()) for transforming and composing generators.
 *
 * @code{.cpp}
 * using namespace hegel::strategies;
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
 * @tparam T The type to generate values for (must be reflect-cpp compatible)
 * @see default_generator() Factory function for creating default type-based
 * Generators
 * @see hegel::strategies Built-in functions that %generate data for you
 */
template <typename T> class Generator : IGenerator<T> {
  public:
    /// @brief Create a Generator from a raw pointer
    Generator(IGenerator<T>* p) : IGenerator<T>(), inner_(p) {}
    /// @brief Create a Generator from a std::shared_ptr
    Generator(std::shared_ptr<IGenerator<T>> p)
        : IGenerator<T>(), inner_(std::move(p)) {}

    /**
     * @brief Generate a random value.
     * @return A randomly generated value of type T
     */
    T generate() const override { return inner_->generate(); }

    /**
     * @brief Get the JSON schema for this generator, if any.
     * @return Optional containing the schema, or nullopt if not schema-based
     */
    const std::optional<std::string_view> schema() const override {
        return inner_->schema();
    }

    /**
     * @brief Transform generated values with a function.
     *
     * Given a Generator&lt;T&gt; and a function from T -> S, creates a
     * Generator&lt;S&gt;.
     *
     * This works by generating values from the Generator&lt;T&gt; and applying
     * a transformation to each value.
     *
     * This is used when you have a function to convert *values* between types.
     * Compare with flatmap().
     *
     * Here's an example of how you'd use this with built-in strategies:
     * @code{.cpp}
     * Generator<double> halved =                           // Result type
     * Generator<double> integers<int>()                                  //
     * Input type Generator<int> .map(
     *             [](int x) { return double(x) / 2.0; }    // transformation:
     * double f(int x)
     *         );
     * @endcode
     *
     * @tparam F Function type (T -> S)
     * @param f Transformation function with signature S f(T)
     * @return Generator&lt;S&gt; producing transformed values
     * @see flatmap()
     */
    template <typename F>
    Generator<std::invoke_result_t<F, T>> map(F&& f) const {
        // F is of type T -> ResultType
        using ResultType = std::invoke_result_t<F, T>;
        auto inner = inner_;
        return from_function<ResultType>(
            [inner, f = std::forward<F>(f)]() -> ResultType {
                return f(inner->generate());
            });
    }

    /**
     * @brief Chain generators for dependent generation.
     *
     * Given a Generator&lt;T&gt; and a function from T -> Generator&lt;S&gt;,
     * creates a Generator&lt;S&gt;. Useful when generation parameters depend on
     * previously generated values.
     *
     * This is used when you have a function that creates a new Generator based
     * on the value. Compare this with map().
     *
     *
     * @code{.cpp}
     * Generator<std::string> sized_string =                     // Result type
     * Generator<std::string> integers<size_t>({.min_value = 1, .max_value =
     * 10})   // Input type Generator<size_t> .flatmap(
     *         [](size_t len) {                                  //
     * transformation Generator<std::string> f(size_t len) return text({ //
     * text() return type is Generator<std::string> .min_size = len, //
     * Constructor parameters to text() depend on the value *len* .max_size =
     * len
     *             });
     *     });
     * @endcode
     *
     * @tparam F Function type (T -> Generator&lt;S&gt;)
     * @param f Function that takes a T and returns a Generator&lt;S&gt;
     * @return Generator&lt;S&gt; producing values from the chained generator
     * @see map(), strategies::text()
     */
    template <typename F> std::invoke_result_t<F, T> flatmap(F&& f) const {
        // Relevant types here:
        //     ResultType: some type
        //     gen_fn_:   () -> T
        //     F:         T -> Generator<ResultType>
        //     Function return type: Generator<ResultType>
        using ResultType =
            decltype(std::declval<std::invoke_result_t<F, T>>().generate());
        auto inner = inner_;
        return from_function<ResultType>(
            [inner, f = std::forward<F>(f)]() -> ResultType {
                return f(inner->generate()).generate();
            });
    }

    /**
     * @brief Filter generated values by a predicate.
     *
     * Creates a Generator that only produces values satisfying the predicate.
     * The new Generator has the same type as this Generator.
     *
     * For performance reasons, if 3 consecutive values fail the predicate,
     * Hegel rejects the test case.
     *
     * So for example, if you want sorted lists of length N, you should generate
     * sorted lists of length N, not generate random lists and filter by a
     * predicate of 'length == N && is_sorted'. (Although the latter is
     * logically correct, it would be a performance nightmare, so Hegel doesn't
     * let you do it that way.)
     *
     * @code{.cpp}
     * Generator<int> even =                                  // Return type is
     * same as input type integers<int>({.min_value = 0, .max_value = 100})  //
     * Input type = Generator<int> .filter(
     *         [](int x) { return x % 2 == 0; }               // bool
     * predicate(int x)
     *     );
     * @endcode
     *
     * @param pred Predicate that values must satisfy. Signature: bool pred(T
     * value)
     * @return Generator&lt;T&gt; producing only values satisfying pred
     */
    Generator<T> filter(std::function<bool(const T&)> pred) const {
        auto inner = inner_;
        return from_function<T>([inner, pred]() -> T {
            for (int i = 0; i < 3; ++i) {
                T value = inner->generate();
                if (pred(value)) {
                    return value;
                }
            }
            internal::stop_test();
        });
    }

  private:
    std::shared_ptr<IGenerator<T>> inner_;
};

/**
 * @brief A generator that uses a function to produce random values of type T.
 *
 * You shouldn't create this directly, but rather use the from_function()
 * function.
 *
 * @tparam T The type of values this generator produces
 */
template <typename T> class FunctionBackedGenerator : public IGenerator<T> {
  public:
    /// @brief Create from a function
    /// @param fn function that will be called repeatedly to generate values
    explicit FunctionBackedGenerator(std::function<T()> fn)
        : gen_fn_(std::move(fn)) {}

    /// @brief Create from a function
    /// @param fn function that will be called repeatedly to generate values
    /// @param schema schema for this generator; not used in generate(), but may
    /// be used when composing this generator
    FunctionBackedGenerator(std::function<T()> fn, std::string schema)
        : gen_fn_(std::move(fn)), schema_(std::move(schema)) {}

    /**
     * @brief Get the JSON schema for this generator, if any.
     * @return Optional containing the schema, or nullopt if not schema-based
     */
    const std::optional<std::string_view> schema() const override {
        return schema_;
    }

    /**
     * @brief Generate a random value.
     * @return A randomly generated value of type T
     */
    T generate() const override { return gen_fn_(); }

  private:
    std::function<T()> gen_fn_;
    std::optional<std::string> schema_;
};

/**
 * @brief A generator that generates data based on a schema.
 *
 * You shouldn't create this directly, but rather use the from_schema() or
 * default_generator() functions.
 *
 * @tparam T The type to generate values for (must be reflect-cpp compatible)
 * @see default_generator() Factory function for creating DefaultGenerators
 */
template <typename T> class SchemaBackedGenerator : public IGenerator<T> {
  public:
    /// @brief Create, given the schema
    SchemaBackedGenerator(std::string schema) : schema_(std::move(schema)) {}

    /// Get const reference to the JSON schema
    const std::optional<std::string_view> schema() const override {
        return std::string_view(schema_);
    }

    /**
     * @brief Generate a random value of type T based on the schema.
     * @return A randomly generated value
     */
    T generate() const override {
        std::string response_json = internal::communicate_with_socket(schema_);

        auto parse_result =
            rfl::json::read<internal::Response<T>>(response_json);
        internal::assume(parse_result.has_value());

        const internal::Response<T>& response = parse_result.value();

        internal::assume(!response.error);
        internal::assume(response.result.has_value());

        return *response.result;
    }

  private:
    std::string schema_;
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
 * struct Person {
 *     std::string name;
 *     int age;
 * };
 *
 * auto gen = hegel::generators::default_generator<Person>();
 * Person p = gen.generate();
 * @endcode
 *
 * @tparam T The type to generate (must be reflect-cpp compatible)
 * @return A SchemaBackedGenerator<T> instance
 */
template <typename T> Generator<T> default_generator() {
    return from_schema<T>(rfl::json::to_schema<T>());
}

/**
 * @brief Construct a generator from a function.
 * @param fn Function that produces values of type T
 */
template <typename T> Generator<T> from_function(std::function<T()> fn) {
    return Generator<T>(new FunctionBackedGenerator<T>(std::move(fn)));
}

/**
 * @brief Construct a generator from a function (with an associated JSON
 * schema).
 * @param fn Function that produces values of type T
 * @param schema JSON schema string for this generator. This isn't used in
 * generate(), but may be used when composing generators.
 */
template <typename T>
Generator<T> from_function(std::function<T()> fn, std::string schema) {
    return Generator<T>(
        new FunctionBackedGenerator<T>(std::move(fn), std::move(schema)));
}

/**
 * @brief Create a from_function from an explicit JSON schema.
 *
 * Use this when you need fine-grained control over the generation
 * schema, or when working with types that don't support automatic
 * schema derivation.
 *
 * @todo link to Hegel schema docs
 *
 * @code{.cpp}
 * auto gen = hegel::generators::from_schema<int>(
 *     R"({"type":"integer","minimum":0,"maximum":100})"
 * );
 * int value = gen.generate();
 * @endcode
 *
 * @tparam T The type to deserialize generated values into
 * @param schema JSON schema string describing the generation constraints
 * @return A Generator<T> that generates according to the schema
 */
template <typename T> Generator<T> from_schema(std::string schema) {
    return Generator<T>(new SchemaBackedGenerator<T>(std::move(schema)));
}

} // namespace hegel::generators
