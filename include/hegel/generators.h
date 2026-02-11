#pragma once

/**
 * @file generators.h
 * @brief Generator, BasicGenerator, and factory functions for Hegel SDK
 *
 * Contains the core Generator<T>, BasicGenerator<T>, and IGenerator<T> class
 * templates, and factory functions.
 */

#include <memory>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include "internal.h"
#include "nlohmann_reader.h"

namespace hegel::internal {
    /// Generate a schema for type T (wrapper around reflect-cpp)
    template <typename T> nlohmann::json type_schema() {
        return nlohmann::json::parse(rfl::json::to_schema<T>());
    }
} // namespace hegel::internal

/**
 * @brief Namespace containing abstractions for data generation.
 *
 * You wouldn't typically use the *classes* in this namespace directly,
 * but rather use the default_generator() function and the strategies
 * in hegel::strategies.
 */
namespace hegel::generators {

    // Forward declarations
    template <typename T> class Generator;
    template <typename T> Generator<T> from_function(std::function<T()> fn);

    // =============================================================================
    // BasicGenerator - Schema with optional client-side transform
    // =============================================================================

    /**
     * @brief A basic generator: a schema plus an optional client-side
     * transform.
     *
     * Basic generators enable schema-based generation even after
     * transformations. The schema is sent to the server, and the transform (if
     * any) is applied client-side to the raw value returned by the server.
     *
     * When map() is called on a basic generator, the transform is composed
     * rather than losing the schema, which is the key optimization.
     *
     * @tparam T The type this generator produces
     */
    template <typename T> class BasicGenerator {
      public:
        /// Create with just a schema (identity transform)
        explicit BasicGenerator(nlohmann::json schema)
            : schema_(std::move(schema)) {}

        /// Create with a schema and a transform
        BasicGenerator(nlohmann::json schema,
                       std::function<T(nlohmann::json&)> transform)
            : schema_(std::move(schema)), transform_(std::move(transform)) {}

        /// Get the schema
        const nlohmann::json& schema() const { return schema_; }

        /// Check if this generator has a transform
        bool has_transform() const { return transform_.has_value(); }

        /// Generate a value by communicating with the server
        T generate() const {
            nlohmann::json response =
                internal::communicate_with_socket(schema_);

            if (response.contains("error")) {
                internal::assume(false);
            }

            internal::assume(response.contains("result"));
            nlohmann::json& result = response["result"];

            if (transform_) {
                return (*transform_)(result);
            } else {
                // Default deserialization
                if constexpr (std::is_arithmetic_v<T> ||
                              std::is_same_v<T, std::string>) {
                    return result.get<T>();
                } else {
                    auto parse_result = internal::read_nlohmann<T>(result);
                    internal::assume(parse_result.has_value());
                    return parse_result.value();
                }
            }
        }

        /// Apply the transform (or default deserialization) to a raw JSON value
        T from_raw(nlohmann::json& raw) const {
            if (transform_) {
                return (*transform_)(raw);
            } else {
                if constexpr (std::is_arithmetic_v<T> ||
                              std::is_same_v<T, std::string>) {
                    return raw.get<T>();
                } else {
                    auto parse_result = internal::read_nlohmann<T>(raw);
                    internal::assume(parse_result.has_value());
                    return parse_result.value();
                }
            }
        }

        /// Compose a transform on top of this basic generator
        template <typename F>
        BasicGenerator<std::invoke_result_t<F, T>> map(F f) const {
            using U = std::invoke_result_t<F, T>;

            if (transform_) {
                auto existing = *transform_;
                return BasicGenerator<U>(
                    schema_,
                    [existing = std::move(existing), f = std::move(f)](
                        nlohmann::json& raw) -> U { return f(existing(raw)); });
            } else {
                // Identity case: deserialize then apply f
                return BasicGenerator<U>(
                    schema_, [f = std::move(f)](nlohmann::json& raw) -> U {
                        T value;
                        if constexpr (std::is_arithmetic_v<T> ||
                                      std::is_same_v<T, std::string>) {
                            value = raw.get<T>();
                        } else {
                            auto parse_result = internal::read_nlohmann<T>(raw);
                            internal::assume(parse_result.has_value());
                            value = parse_result.value();
                        }
                        return f(std::move(value));
                    });
            }
        }

      private:
        nlohmann::json schema_;
        std::optional<std::function<T(nlohmann::json&)>> transform_;
    };

    // =============================================================================
    // IGenerator - Base interface
    // =============================================================================

    /**
     * @brief The base interface that defines Generators.
     *
     * IGenerator's core method is generate(), which produces a single value of
     * type T.
     *
     * IGenerator also has as_basic() which returns an optional BasicGenerator
     * for schema-based generation with optional transforms.
     *
     * @tparam T The type to generate values for
     * @see Generator Sub-class that implements IGenerator
     * @see hegel::strategies Built-in functions that %generate data for you
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
         * @brief Convert this generator to a basic generator, if possible.
         *
         * A basic generator has a schema and an optional client-side
         * transform. When available, this enables single-request schema-based
         * generation and allows combinators to compose schemas.
         *
         * Returns nullopt for generators that cannot be expressed as a schema
         * (e.g., after flat_map or filter).
         */
        virtual std::optional<BasicGenerator<T>> as_basic() const {
            return std::nullopt;
        }
    };

    // =============================================================================
    // Generator - Main user-facing class
    // =============================================================================

    /**
     * @brief A Generator; this is the class that most methods return.
     *
     * Generator is the core abstraction for random data generation. It wraps
     * an IGenerator (which produces values) and provides combinators (map(),
     * flatmap(), filter()) for transforming and composing generators.
     *
     * @code{.cpp}
     * using namespace hegel::strategies;
     *
     * // Create a generator
     * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
     * int value = int_gen.generate();
     *
     * // Transform with map (preserves schema via BasicGenerator)
     * auto squared = int_gen.map([](int x) { return x * x; });
     *
     * // Filter values
     * auto even = int_gen.filter([](int x) { return x % 2 == 0; });
     * @endcode
     *
     * @tparam T The type to generate values for
     * @see hegel::strategies Built-in functions that %generate data for you
     */
    template <typename T> class Generator : public IGenerator<T> {
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
         * @brief Convert to a basic generator, if possible.
         */
        std::optional<BasicGenerator<T>> as_basic() const override {
            return inner_->as_basic();
        }

        /**
         * @brief Transform generated values with a function.
         *
         * If this generator is basic, the resulting generator preserves
         * the schema by composing the transform (via BasicGenerator::map).
         * Otherwise, falls back to a function-based generator.
         *
         * @tparam F Function type (T -> S)
         * @param f Transformation function
         * @return Generator<S> producing transformed values
         * @see flatmap()
         */
        template <typename F>
        Generator<std::invoke_result_t<F, T>> map(F&& f) const {
            using ResultType = std::invoke_result_t<F, T>;

            auto basic = inner_->as_basic();
            if (basic) {
                return from_basic<ResultType>(basic->map(f));
            }

            auto inner = inner_;
            return from_function<ResultType>(
                [inner, f = std::forward<F>(f)]() -> ResultType {
                    return f(inner->generate());
                });
        }

        /**
         * @brief Chain generators for dependent generation.
         *
         * Given a Generator<T> and a function from T -> Generator<S>,
         * creates a Generator<S>. Always loses the schema.
         *
         * @tparam F Function type (T -> Generator<S>)
         * @param f Function that takes a T and returns a Generator<S>
         * @return Generator<S> producing values from the chained generator
         * @see map()
         */
        template <typename F> std::invoke_result_t<F, T> flatmap(F&& f) const {
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
         * Always loses the schema. For performance, if 3 consecutive values
         * fail the predicate, Hegel rejects the test case.
         *
         * @param pred Predicate that values must satisfy
         * @return Generator<T> producing only values satisfying pred
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

    // =============================================================================
    // Concrete generator implementations
    // =============================================================================

    /**
     * @brief A generator that uses a function to produce random values.
     *
     * @tparam T The type of values this generator produces
     */
    template <typename T> class FunctionBackedGenerator : public IGenerator<T> {
      public:
        /// @brief Create from a function
        explicit FunctionBackedGenerator(std::function<T()> fn)
            : gen_fn_(std::move(fn)) {}

        /// @brief Create from a function with an associated basic generator
        FunctionBackedGenerator(std::function<T()> fn, BasicGenerator<T> basic)
            : gen_fn_(std::move(fn)), basic_(std::move(basic)) {}

        std::optional<BasicGenerator<T>> as_basic() const override {
            return basic_;
        }

        T generate() const override { return gen_fn_(); }

      private:
        std::function<T()> gen_fn_;
        std::optional<BasicGenerator<T>> basic_;
    };

    /**
     * @brief A generator backed by a BasicGenerator.
     *
     * Uses the BasicGenerator directly for generation - sends schema to server
     * and applies optional transform.
     *
     * @tparam T The type to generate values for
     */
    template <typename T> class BasicBackedGenerator : public IGenerator<T> {
      public:
        explicit BasicBackedGenerator(BasicGenerator<T> basic)
            : basic_(std::move(basic)) {}

        std::optional<BasicGenerator<T>> as_basic() const override {
            return basic_;
        }

        T generate() const override { return basic_.generate(); }

      private:
        BasicGenerator<T> basic_;
    };

    // =============================================================================
    // Factory functions
    // =============================================================================

    /**
     * @brief Create a generator for type T using automatic schema derivation.
     *
     * Uses reflect-cpp to derive a schema from the type's structure.
     *
     * @tparam T The type to generate (must be reflect-cpp compatible)
     * @return A Generator<T> instance
     */
    template <typename T> Generator<T> default_generator() {
        return from_basic<T>(BasicGenerator<T>(internal::type_schema<T>()));
    }

    /**
     * @brief Construct a generator from a function.
     * @param fn Function that produces values of type T
     */
    template <typename T> Generator<T> from_function(std::function<T()> fn) {
        return Generator<T>(new FunctionBackedGenerator<T>(std::move(fn)));
    }

    /**
     * @brief Construct a generator from a function with a basic generator.
     * @param fn Function that produces values of type T
     * @param basic BasicGenerator for schema composition
     */
    template <typename T>
    Generator<T> from_function(std::function<T()> fn, BasicGenerator<T> basic) {
        return Generator<T>(
            new FunctionBackedGenerator<T>(std::move(fn), std::move(basic)));
    }

    /**
     * @brief Create a generator from a BasicGenerator.
     *
     * Uses the BasicGenerator directly for both generation and composition.
     *
     * @tparam T The type to generate
     * @param basic The BasicGenerator to wrap
     * @return A Generator<T> backed by the BasicGenerator
     */
    template <typename T> Generator<T> from_basic(BasicGenerator<T> basic) {
        return Generator<T>(new BasicBackedGenerator<T>(std::move(basic)));
    }

    /**
     * @brief Create a generator from an explicit schema.
     *
     * Creates a BasicGenerator with the schema and wraps it.
     *
     * @tparam T The type to deserialize generated values into
     * @param schema Schema describing the generation constraints
     * @return A Generator<T> that generates according to the schema
     */
    template <typename T> Generator<T> from_schema(nlohmann::json schema) {
        return from_basic<T>(BasicGenerator<T>(std::move(schema)));
    }

} // namespace hegel::generators
