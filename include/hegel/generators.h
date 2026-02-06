#pragma once

/**
 * @file generators.h
 * @brief Generator and SchemaBackedGenerator class templates for Hegel SDK
 *
 * Contains the core Generator<T> and SchemaBackedGenerator<T> class templates,
 * the Response wrapper, and factory functions.
 */

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <memory>

#include "internal.h"

namespace hegel {
    template <typename T> class Generator;  // Forward decl

    template <typename T>
    struct IGenerator {
        IGenerator() {}
        virtual ~IGenerator() = default;

        /**
        * @brief Generate a random value.
        * @return A randomly generated value of type T
        */
        virtual T generate() const = 0;

        /**
        * @brief Get the JSON schema for this generator, if any.
        * @return Optional containing the schema, or nullopt if not schema-based
        */
        virtual const std::optional<std::string_view> schema() const = 0;
    };

    template <typename T>
    class Generator : IGenerator<T> {
    public:
        Generator(IGenerator<T>* p) : IGenerator<T>(), inner_(p) {}
        Generator(std::shared_ptr<IGenerator<T>> p) : IGenerator<T>(), inner_(std::move(p)) {}

        /**
        * @brief Generate a random value.
        * @return A randomly generated value of type T
        */
        T generate() const override { return inner_->generate(); }

        /**
        * @brief Get the JSON schema for this generator, if any.
        * @return Optional containing the schema, or nullopt if not schema-based
        */
        const std::optional<std::string_view> schema() const override { return inner_->schema(); }

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
        Generator<std::invoke_result_t<F, T>> map(F&& f) const {
            // F is of type T -> ResultType
            using ResultType = std::invoke_result_t<F, T>;
            auto inner = inner_;
            return generator<ResultType>([inner, f = std::forward<F>(f)]() -> ResultType { return f(inner->generate()); });
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
        std::invoke_result_t<F, T> flatmap(F&& f) const {
            // Relevant types here:
            //     ResultType: some type
            //     gen_fn_:   () -> T
            //     F:         T -> Generator<ResultType>
            //     Function return type: Generator<ResultType>
            using ResultType = decltype(std::declval<std::invoke_result_t<F, T>>().generate());
            auto inner = inner_;
            return generator<ResultType>(
                [inner, f = std::forward<F>(f)]() -> ResultType { return f(inner->generate()).generate(); });
        }

        /**
        * @brief Filter generated values by a predicate.
        *
        * Creates a generator that only produces values satisfying the predicate.
        * If 3 consecutive values fail the predicate, calls assume(false)
        * to reject the test case.
        *
        * @code{.cpp}
        * auto even = integers<int>({.min_value = 0, .max_value = 100})
        *     .filter([](int x) { return x % 2 == 0; });
        * @endcode
        *
        * @param pred Predicate that values must satisfy
        * @return Generator<T> producing only values satisfying pred
        */
        Generator<T> filter(std::function<bool(const T&)> pred) const {
            auto inner = inner_;
            return generator<T>([inner, pred]() -> T {
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
    * @tparam T The type of values this generator produces
    */
    template <typename T>
    class FunctionBackedGenerator : public IGenerator<T> {
    public:
        explicit FunctionBackedGenerator(std::function<T()> fn) : gen_fn_(std::move(fn)) {}

        FunctionBackedGenerator(std::function<T()> fn, std::string schema)
            : gen_fn_(std::move(fn)), schema_(std::move(schema)) {}

        /**
        * @brief Get the JSON schema for this generator, if any.
        * @return Optional containing the schema, or nullopt if not schema-based
        */
        const std::optional<std::string_view> schema() const override { return schema_; }

        /**
        * @brief Generate a random value.
        * @return A randomly generated value of type T
        */
        T generate() const override { return gen_fn_(); }

    private:
        std::function<T()> gen_fn_;
        std::optional<std::string> schema_;
    };

    // =============================================================================
    // SchemaBackedGenerator class template
    // =============================================================================

    /**
    * @brief A generator that derives its schema from type T using reflect-cpp.
    *
    * SchemaBackedGenerator automatically creates a JSON schema from the structure
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
    class SchemaBackedGenerator : public IGenerator<T> {
    public:
        SchemaBackedGenerator(std::string schema) : schema_(std::move(schema)) {}

        /// Get const reference to the JSON schema
        const std::optional<std::string_view> schema() const override { return std::string_view(schema_); }

        /**
        * @brief Generate a random value of type T.
        * @return A randomly generated value
        */
        T generate() const override { 
            std::string response_json = internal::communicate_with_socket(schema_);

            auto parse_result = rfl::json::read<internal::Response<T>>(response_json);
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
    * struct Point { double x; double y; };
    * auto gen = hegel::default_generator<Point>();
    * Point p = gen.generate();
    * @endcode
    *
    * @tparam T The type to generate (must be reflect-cpp compatible)
    * @return A SchemaBackedGenerator<T> instance
    */
    template <typename T>
    SchemaBackedGenerator<T> default_generator() {
        return from_schema<T>(rfl::json::to_schema<T>());
    }

    /**
    * @brief Construct a generator from a function.
    * @param fn Function that produces values of type T
    */
    template <typename T>
    Generator<T> generator(std::function<T()> fn) {
        return Generator<T>(new FunctionBackedGenerator<T>(std::move(fn)));
    }

    /**
    * @brief Construct a generator with an associated JSON schema.
    * @param fn Function that produces values of type T
    * @param schema JSON schema string for this generator
    */
    template <typename T>
    Generator<T> generator(std::function<T()> fn, std::string schema) {
        return Generator<T>(new FunctionBackedGenerator<T>(std::move(fn), std::move(schema)));
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
        return Generator<T>(new SchemaBackedGenerator<T>(std::move(schema)));
    }

}  // namespace hegel
