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

    // Convenience alias used throughout this header
    using TestCaseData = impl::data::TestCaseData;

    /**
     * @brief The base interface that defines Generators.
     *
     * IGenerator's core method is do_draw(), which produces a single value
     * of type T given the current test case data.
     *
     * IGenerator also has a schema() method that returns the schema (if there
     * is one).
     *
     * It's rare that you would need to implement IGenerator yourself; you would
     * typically use pre-existing primitives from the library or compose them
     * using compositors from the library. Even the library only uses IGenerator
     * sparingly; most operations are exposed in Generator, a class that
     * implements IGenerator.
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

        /**
         * @brief Draw a random value.
         * @param data The current test case data
         * @return A randomly generated value of type T
         */
        virtual T do_draw(TestCaseData* data) const = 0;

        /**
         * @brief Get the CBOR schema for this generator, if any.
         *
         * All IGenerators *may* have a schema, even if the schema isn't
         * directly used for generating; this functionality may be used for
         * composing generators, for example.
         *
         * @return Optional containing the schema, or nullopt if not
         * schema-based
         */
        virtual std::optional<hegel::internal::json::json> schema() const = 0;

        /// @cond INTERNAL
        /**
         * @brief Get a function that applies this generator's full
         * transformation to a pre-fetched raw JSON value.
         *
         * Used by one_of to build tagged-tuple schemas that allow a single
         * server round-trip for generators that include a client-side
         * transform (e.g. after map()).  Returns nullopt for generators whose
         * transform cannot be separated from their server request (e.g.
         * filter, flat_map).
         */
        virtual std::optional<
            std::function<T(hegel::internal::json::json_raw_ref)>>
        get_applier() const {
            return std::nullopt;
        }

        /**
         * @brief Returns true if this generator applies a client-side
         * transform to the server's value (i.e., created via map()).
         *
         * Distinguishes Path 1 (schema-backed, no transform) from Path 2
         * (schema-backed with transform) in one_of composition.
         */
        virtual bool has_client_transform() const noexcept { return false; }
        /// @endcond
    };

    // =============================================================================
    // FunctionBackedGenerator
    // =============================================================================

    /**
     * @brief A generator that uses a function to produce random values of type
     * T.
     *
     * You shouldn't create this directly, but rather use the from_function()
     * function.
     *
     * @tparam T The type of values this generator produces
     */
    template <typename T> class FunctionBackedGenerator : public IGenerator<T> {
      public:
        /// @brief Create from a function
        /// @param fn function that will be called repeatedly to draw values
        explicit FunctionBackedGenerator(std::function<T(TestCaseData*)> fn)
            : gen_fn_(std::move(fn)) {}

        /// @brief Create from a function
        /// @param fn function that will be called repeatedly to draw values
        /// @param schema schema for this generator; not used in do_draw(), but
        /// may be used when composing this generator
        FunctionBackedGenerator(std::function<T(TestCaseData*)> fn,
                                hegel::internal::json::json schema)
            : gen_fn_(std::move(fn)), schema_(std::move(schema)) {}

        /**
         * @brief Get the CBOR schema for this generator, if any.
         * @return Optional containing the schema, or nullopt if not
         * schema-based
         */
        std::optional<hegel::internal::json::json> schema() const override {
            return schema_;
        }

        /**
         * @brief Draw a random value.
         * @param data The current test case data
         * @return A randomly generated value of type T
         */
        T do_draw(TestCaseData* data) const override { return gen_fn_(data); }

      private:
        std::function<T(TestCaseData*)> gen_fn_;
        std::optional<hegel::internal::json::json> schema_;
    };

    // =============================================================================
    // Deserialization helper
    // =============================================================================

    /// @cond INTERNAL
    namespace detail {

        /// Deserialize a raw JSON value into T using the same rules as
        /// SchemaBackedGenerator::do_draw().
        template <typename T>
        T deserialize_raw(hegel::internal::json::json_raw_ref result) {
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

    } // namespace detail
    /// @endcond

    // =============================================================================
    // SchemaBackedGenerator
    // =============================================================================

    /**
     * @brief A generator that generates data based on a schema.
     *
     * You shouldn't create this directly, but rather use the from_schema() or
     * default_generator() functions.
     *
     * @tparam T The type to generate values for (must be reflect-cpp
     * compatible)
     * @see default_generator() Factory function for creating DefaultGenerators
     */
    template <typename T> class SchemaBackedGenerator : public IGenerator<T> {
      public:
        /// @brief Create, given the schema
        SchemaBackedGenerator(hegel::internal::json::json schema)
            : schema_(std::move(schema)) {}

        /// Get the CBOR schema
        std::optional<hegel::internal::json::json> schema() const override {
            return schema_;
        }

        /**
         * @brief Draw a random value of type T based on the schema.
         * @param data The current test case data
         * @return A randomly generated value
         */
        T do_draw(TestCaseData* data) const override {
            hegel::internal::json::json response =
                internal::communicate_with_core(schema_, data);

            if (!response.contains("result")) {
                throw std::runtime_error(
                    "Server response missing 'result' field");
            }
            return detail::deserialize_raw<T>(response["result"]);
        }

        /// @cond INTERNAL
        std::optional<std::function<T(hegel::internal::json::json_raw_ref)>>
        get_applier() const override {
            return [](hegel::internal::json::json_raw_ref raw) -> T {
                return detail::deserialize_raw<T>(raw);
            };
        }
        /// @endcond

      private:
        hegel::internal::json::json schema_;
    };

    // =============================================================================
    // MappedSchemaGenerator
    // =============================================================================

    /// @cond INTERNAL
    /**
     * @brief A generator backed by a schema with an additional client-side
     * transform.
     *
     * Created by Generator::map() when the source generator has a schema.
     * Preserves the raw schema so one_of can build tagged-tuple schemas,
     * enabling a single server round-trip even for transformed branches.
     */
    template <typename T> class MappedSchemaGenerator : public IGenerator<T> {
      public:
        MappedSchemaGenerator(
            hegel::internal::json::json schema,
            std::function<T(hegel::internal::json::json_raw_ref)> applier)
            : schema_(std::move(schema)), applier_(std::move(applier)) {}

        T do_draw(TestCaseData* data) const override {
            hegel::internal::json::json response =
                internal::communicate_with_core(schema_, data);
            if (!response.contains("result")) {
                throw std::runtime_error(
                    "Server response missing 'result' field");
            }
            return applier_(response["result"]);
        }

        std::optional<hegel::internal::json::json> schema() const override {
            return schema_;
        }

        std::optional<std::function<T(hegel::internal::json::json_raw_ref)>>
        get_applier() const override {
            return applier_;
        }

        bool has_client_transform() const noexcept override { return true; }

      private:
        hegel::internal::json::json schema_;
        std::function<T(hegel::internal::json::json_raw_ref)> applier_;
    };
    /// @endcond

    // =============================================================================
    // Generator
    // =============================================================================

    /**
     * @brief A Generator; this is the class that most methods return.
     *
     * Generator is the core abstraction for random data generation. It wraps
     * an IGenerator (which produces values) and provides combinators  (map(),
     * flat_map(), filter()) for transforming and composing generators.
     *
     * @code{.cpp}
     * using namespace hegel::generators;
     *
     * // Create a generator and draw a value
     * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
     * int value = hegel::draw(int_gen);
     *
     * // Transform with map
     * auto squared = int_gen.map([](int x) { return x * x; });
     *
     * // Filter values
     * auto even = int_gen.filter([](int x) { return x % 2 == 0; });
     *
     * // Dependent generation with flat_map
     * auto sized = integers<size_t>({.min_value = 1, .max_value = 10})
     *     .flat_map([](size_t len) {
     *         return text({.min_size = len, .max_size = len});
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
        /// @brief Create a Generator from a raw pointer
        Generator(IGenerator<T>* p) : IGenerator<T>(), inner_(p) {}
        /// @brief Create a Generator from a std::shared_ptr
        Generator(std::shared_ptr<IGenerator<T>> p)
            : IGenerator<T>(), inner_(std::move(p)) {}

        /**
         * @brief Draw a random value (internal).
         * @param data The current test case data
         * @return A randomly generated value of type T
         */
        T do_draw(TestCaseData* data) const override {
            return inner_->do_draw(data);
        }

        /**
         * @brief Get the CBOR schema for this generator, if any.
         * @return Optional containing the schema, or nullopt if not
         * schema-based
         */
        std::optional<hegel::internal::json::json> schema() const override {
            return inner_->schema();
        }

        /// @cond INTERNAL
        std::optional<std::function<T(hegel::internal::json::json_raw_ref)>>
        get_applier() const {
            return inner_->get_applier();
        }

        bool has_client_transform() const noexcept {
            return inner_->has_client_transform();
        }
        /// @endcond

        /**
         * @brief Transform generated values with a function.
         *
         * Given a Generator&lt;T&gt; and a function from T -> S, creates a
         * Generator&lt;S&gt;.
         *
         * This works by generating values from the Generator&lt;T&gt; and
         * applying a transformation to each value.
         *
         * This is used when you have a function to convert *values* between
         * types. Compare with flat_map().
         *
         * Here's an example of how you'd use this with built-in strategies:
         * @code{.cpp}
         * Generator<double> halved =                           // Result type
         * Generator<double> integers<int>()                                  //
         * Input type Generator<int> .map(
         *             [](int x) { return double(x) / 2.0; }    //
         * transformation: double f(int x)
         *         );
         * @endcode
         *
         * @tparam F Function type (T -> S)
         * @param f Transformation function with signature S f(T)
         * @return Generator&lt;S&gt; producing transformed values
         * @see flat_map()
         */
        template <typename F>
        Generator<std::invoke_result_t<F, T>> map(F&& f) const {
            // F is of type T -> ResultType
            using ResultType = std::invoke_result_t<F, T>;
            auto maybe_applier = inner_->get_applier();
            auto maybe_schema = inner_->schema();
            if (maybe_applier && maybe_schema) {
                // Compose: deserialize/transform raw value, then apply f.
                // Preserves the schema so one_of can build tagged-tuple
                // schemas with a single server round-trip.
                auto applier = *maybe_applier;
                auto composed =
                    [applier,
                     f](hegel::internal::json::json_raw_ref raw) -> ResultType {
                    return f(applier(raw));
                };
                return Generator<ResultType>(
                    new MappedSchemaGenerator<ResultType>(*maybe_schema,
                                                          std::move(composed)));
            }
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
         * Given a Generator&lt;T&gt; and a function from T ->
         * Generator&lt;S&gt;, creates a Generator&lt;S&gt;. Useful when
         * generation parameters depend on previously generated values.
         *
         * This is used when you have a function that creates a new Generator
         * based on the value. Compare this with map().
         *
         *
         * @code{.cpp}
         * Generator<std::string> sized_string =                     // Result
         * type Generator<std::string> integers<size_t>({.min_value = 1,
         * .max_value = 10})   // Input type Generator<size_t> .flat_map(
         *         [](size_t len) {                                  //
         * transformation Generator<std::string> f(size_t len) return text({ //
         * text() return type is Generator<std::string> .min_size = len, //
         * Constructor parameters to text() depend on the value *len* .max_size
         * = len
         *             });
         *     });
         * @endcode
         *
         * @tparam F Function type (T -> Generator&lt;S&gt;)
         * @param f Function that takes a T and returns a Generator&lt;S&gt;
         * @return Generator&lt;S&gt; producing values from the chained
         * generator
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
         * If many consecutive values fail the predicate, Hegel rejects the
         * test case. Keep filters selective: use direct generation with
         * appropriate constraints rather than generating broadly and filtering
         * most values away.
         *
         * @code{.cpp}
         * Generator<int> even =                                  // Return type
         * is same as input type integers<int>({.min_value = 0, .max_value =
         * 100})  // Input type = Generator<int> .filter(
         *         [](int x) { return x % 2 == 0; }               // bool
         * predicate(int x)
         *     );
         * @endcode
         *
         * @param pred Predicate that values must satisfy. Signature: bool
         * pred(T value)
         * @return Generator&lt;T&gt; producing only values satisfying pred
         */
        Generator<T> filter(std::function<bool(const T&)> pred) const {
            auto inner = inner_;
            return from_function<T>([inner, pred](TestCaseData* data) -> T {
                for (int i = 0; i < 3; ++i) {
                    internal::start_span(12, data);
                    T value = inner->do_draw(data);
                    if (pred(value)) {
                        internal::stop_span(false, data);
                        return value;
                    }
                    internal::stop_span(true, data);
                }
                internal::stop_test();
            });
        }

      private:
        std::shared_ptr<IGenerator<T>> inner_;
    };

    // =============================================================================
    // Factory functions
    // =============================================================================

    /**
     * @brief Construct a generator from a function.
     * @param fn Function that produces values of type T given test case data
     */
    template <typename T>
    Generator<T> from_function(std::function<T(TestCaseData*)> fn) {
        return Generator<T>(new FunctionBackedGenerator<T>(std::move(fn)));
    }

    /**
     * @brief Construct a generator from a function (with an associated CBOR
     * schema).
     * @param fn Function that produces values of type T given test case data
     * @param schema CBOR schema for this generator. This isn't used in
     * do_draw(), but may be used when composing generators.
     */
    template <typename T>
    Generator<T> from_function(std::function<T(TestCaseData*)> fn,
                               hegel::internal::json::json schema) {
        return Generator<T>(
            new FunctionBackedGenerator<T>(std::move(fn), std::move(schema)));
    }

    /**
     * @brief Create a generator from an explicit CBOR schema.
     *
     * Use this when you need fine-grained control over the generation
     * schema, or when working with types that don't support automatic
     * schema derivation.
     *
     * @todo link to Hegel schema docs
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
