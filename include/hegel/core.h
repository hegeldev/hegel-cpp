#pragma once

/**
 * @file core.h
 * @brief Generator and SchemaBackedGenerator class templates for Hegel
 *
 * Contains the core Generator<T> and SchemaBackedGenerator<T> class templates,
 * the Response wrapper, and factory functions.
 */

#include <memory>

#include "internal.h"
#include "nlohmann_reader.h"

/**
 * @brief Namespace containing abstractions for data generation.
 *
 * You wouldn't typically use the *classes* in this namespace directly,
 * but rather use the default_generator() function and the strategies
 * in hegel::strategies.
 */
namespace hegel::generators {

    /// @cond INTERNAL
    // Convenience alias used throughout this header
    using TestCaseData = impl::test_case::TestCaseData;
    /// @endcond

    /**
     * @brief The base interface that defines Generators.
     *
     * You should never need to implement IGenerator yourself.
     *
     * @tparam T The type to generate values for (must be reflect-cpp
     * compatible)
     * @see default_generator() Factory function for creating default type-based
     * Generators
     * @see hegel::generators Built-in functions that %generate data for you
     * @see Generator Sub-class that implements IGenerator; the thing you
     * typically interact with
     */
    template <typename T> struct IGenerator {
        IGenerator() {}
        virtual ~IGenerator() = default;

        /// @cond INTERNAL
        virtual T do_draw(TestCaseData* data) const = 0;
        // Get the CBOR schema for this generator, if any.
        //
        // All IGenerators *may* have a schema, even if the schema isn't
        // directly used for generating; this functionality may be used for
        // composing generators, for example.

        // Returns an pptional containing the schema, or nullopt if not
        // schema-based
        virtual std::optional<hegel::internal::json::json> schema() const = 0;
        /// @endcond
    };

    /**
     * @brief A Generator; this is the class that most methods return.
     *
     * Generator is the core abstraction for random data generation. It wraps
     * an IGenerator (which produces values) and provides combinators  (map(),
     * flat_map(), filter()) for transforming and composing generators.
     *
     * @code{.cpp}
     * namespace gs = hegel::generators;
     *
     * // Create a generator and draw a value
     * auto int_gen = gs::integers<int>({.min_value = 0, .max_value = 100});
     * int value = hegel::draw(int_gen);
     *
     * // Transform with map
     * auto squared = int_gen.map([](int x) { return x * x; });
     *
     * // Filter values
     * auto even = int_gen.filter([](int x) { return x % 2 == 0; });
     *
     * // Dependent generation with flat_map
     * auto sized = gs::integers<size_t>({.min_value = 1, .max_value = 10})
     *     .flat_map([](size_t len) {
     *         return gs::text({.min_size = len, .max_size = len});
     *     });
     * @endcode
     *
     * @tparam T The type to generate values for (must be reflect-cpp
     * compatible)
     * @see default_generator() Factory function for creating default type-based
     * Generators
     * @see hegel::generators Built-in functions that %generate data for you
     */
    template <typename T> class Generator : IGenerator<T> {
      public:
        /// @cond INTERNAL
        Generator(IGenerator<T>* p) : IGenerator<T>(), inner_(p) {}
        Generator(std::shared_ptr<IGenerator<T>> p)
            : IGenerator<T>(), inner_(std::move(p)) {}

        T do_draw(TestCaseData* data) const override {
            return inner_->do_draw(data);
        }

        std::optional<hegel::internal::json::json> schema() const override {
            return inner_->schema();
        }
        /// @endcond

        /**
         * @brief Transform generated values with a function.
         *
         * Given a Generator\<T\> and a function T -> S, creates a
         * Generator\<S\>. This is used when you have a function to convert
         * *values* between types. Compare with flat_map().
         *
         * @code{.cpp}
         * auto halved = integers<int>().map(
         *     [](int x) { return double(x) / 2.0; }
         * );
         * // halved is Generator<double>
         * @endcode
         *
         * @tparam F Function type (T -> S)
         * @param f Transformation function with signature S f(T)
         * @return Generator\<S\> producing transformed values
         * @see flat_map()
         */
        template <typename F>
        Generator<std::invoke_result_t<F, T>> map(F&& f) const {
            // F is of type T -> ResultType
            using ResultType = std::invoke_result_t<F, T>;
            auto inner = inner_;
            return from_function<ResultType>(
                [inner,
                 f = std::forward<F>(f)](TestCaseData* data) -> ResultType {
                    return f(inner->do_draw(data));
                });
        }

        /**
         * @brief Chain generators for dependent generation.
         *
         * Given a Generator\<T\> and a function T -> Generator\<S\>, creates a
         * Generator\<S\>. Useful when generation parameters depend on
         * previously generated values. Compare with map().
         *
         * @code{.cpp}
         * auto sized_string =
         *     integers<size_t>({.min_value = 1, .max_value = 10})
         *         .flat_map([](size_t len) {
         *             return text({.min_size = len, .max_size = len});
         *         });
         * // sized_string is Generator<std::string>
         * @endcode
         *
         * @tparam F Function type (T -> Generator\<S\>)
         * @param f Function that takes a T and returns a Generator\<S\>
         * @return Generator\<S\> producing values from the chained generator
         * @see map(), text()
         */
        template <typename F> std::invoke_result_t<F, T> flat_map(F&& f) const {
            // Relevant types here:
            //     ResultType: some type
            //     gen_fn_:   () -> T
            //     F:         T -> Generator<ResultType>
            //     Function return type: Generator<ResultType>
            using ResultType =
                decltype(std::declval<std::invoke_result_t<F, T>>().do_draw(
                    std::declval<TestCaseData*>()));
            auto inner = inner_;
            return from_function<ResultType>(
                [inner,
                 f = std::forward<F>(f)](TestCaseData* data) -> ResultType {
                    return f(inner->do_draw(data)).do_draw(data);
                });
        }

        /**
         * @brief Filter generated values by a predicate.
         *
         * Creates a Generator that only produces values satisfying the
         * predicate. The new Generator has the same type as this Generator.
         *
         * For performance reasons, if 3 consecutive values fail the predicate,
         * Hegel rejects the test case. You should prefer generating values that
         * naturally satisfy your constraints rather than filtering broadly.
         *
         * @code{.cpp}
         * auto even = integers<int>({.min_value = 0, .max_value = 100})
         *     .filter([](int x) { return x % 2 == 0; });
         * // even is Generator<int>
         * @endcode
         *
         * @param pred Predicate that values must satisfy
         * @return Generator<T> producing only values satisfying pred
         */
        Generator<T> filter(std::function<bool(const T&)> pred) const {
            auto inner = inner_;
            return from_function<T>([inner, pred](TestCaseData* data) -> T {
                for (int i = 0; i < 3; ++i) {
                    T value = inner->do_draw(data);
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

    /// @cond INTERNAL
    // Generator that produces values by invoking a user-provided function.
    // Users never construct or reference this type directly; it's produced
    // internally by from_function() and by map()/flat_map()/filter().
    template <typename T> class FunctionBackedGenerator : public IGenerator<T> {
      public:
        explicit FunctionBackedGenerator(std::function<T(TestCaseData*)> fn)
            : gen_fn_(std::move(fn)) {}

        FunctionBackedGenerator(std::function<T(TestCaseData*)> fn,
                                hegel::internal::json::json schema)
            : gen_fn_(std::move(fn)), schema_(std::move(schema)) {}

        std::optional<hegel::internal::json::json> schema() const override {
            return schema_;
        }

        T do_draw(TestCaseData* data) const override { return gen_fn_(data); }

      private:
        std::function<T(TestCaseData*)> gen_fn_;
        std::optional<hegel::internal::json::json> schema_;
    };
    /// @endcond

    /// @cond INTERNAL
    // Generator that drives value generation through the Hegel protocol by
    // sending a CBOR schema to the server. Users never construct or reference
    // this type directly; it's produced internally by from_schema(),
    // default_generator(), and the schema-based primitives.
    template <typename T> class SchemaBackedGenerator : public IGenerator<T> {
      public:
        SchemaBackedGenerator(hegel::internal::json::json schema)
            : schema_(std::move(schema)) {}

        std::optional<hegel::internal::json::json> schema() const override {
            return schema_;
        }

        T do_draw(TestCaseData* data) const override {
            hegel::internal::json::json response =
                internal::communicate_with_core(schema_, data);

            // Extract the result value
            if (!response.contains("result")) {
                throw std::runtime_error(
                    "Server response missing 'result' field");
            }
            hegel::internal::json::json_raw_ref result = response["result"];

            if constexpr (std::is_same_v<T, std::string>) {
                return result.get_string();
            } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
                return result.get_bool();
            } else if constexpr (std::is_floating_point_v<T>) {
                return static_cast<T>(result.get_double());
            } else if constexpr (std::is_unsigned_v<T>) {
                return static_cast<T>(result.get_uint64_t());
            } else if constexpr (std::is_integral_v<T>) {
                return static_cast<T>(result.get_int64_t());
            } else {
                auto parse_result = internal::read_nlohmann<T>(result);
                if (!parse_result.has_value()) {
                    throw std::runtime_error(
                        "Failed to parse server response into "
                        "requested type");
                }
                return parse_result.value();
            }
        }

      private:
        hegel::internal::json::json schema_;
    };
    /// @endcond

    /// @cond INTERNAL
    // Construct a generator from a function. `fn` produces values of type T
    // given test case data. The second overload associates a CBOR schema with
    // the generator; the schema isn't used in do_draw(), but may be used when
    // composing generators.
    template <typename T>
    Generator<T> from_function(std::function<T(TestCaseData*)> fn) {
        return Generator<T>(new FunctionBackedGenerator<T>(std::move(fn)));
    }

    template <typename T>
    Generator<T> from_function(std::function<T(TestCaseData*)> fn,
                               hegel::internal::json::json schema) {
        return Generator<T>(
            new FunctionBackedGenerator<T>(std::move(fn), std::move(schema)));
    }
    /// @endcond

    /**
     * @brief Create a generator from an explicit CBOR schema.
     *
     * Use this when you need fine-grained control over the generation
     * schema, or when working with types that don't support automatic
     * schema derivation.
     *
     * See: https://hegel.dev/reference/protocol#schemas
     *
     * @code{.cpp}
     * auto gen = hegel::generators::from_schema<int>(
     *     hegel::internal::json::json{{"type", "integer"},
     *                    {"min_value", 0},
     *                    {"max_value", 100}}
     * );
     * int value = hegel::draw(gen);
     * @endcode
     *
     * @tparam T The type to deserialize generated values into
     * @param schema CBOR schema describing the generation constraints
     * @return A Generator<T> that generates according to the schema
     */
    template <typename T>
    Generator<T> from_schema(hegel::internal::json::json schema) {
        return Generator<T>(new SchemaBackedGenerator<T>(std::move(schema)));
    }

} // namespace hegel::generators
