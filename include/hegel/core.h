#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "internal.h"
#include "nlohmann_reader.h"
#include "test_case.h"

/**
 * @brief Hegel generators.
 */
namespace hegel::generators {

    template <typename T> struct BasicGenerator;
    template <typename T> class CompositeGenerator;
    template <typename T, typename U> class MappedGenerator;

    /// @cond INTERNAL
    // Default client-side parser used by schema-backed generators whose parse
    // step is determined solely by T. Primitives use typed accessors on the
    // raw json_raw_ref; reflectable composite types fall back to reflect-cpp.
    template <typename T>
    T default_parse_raw(const hegel::internal::json::json_raw_ref& result) {
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
                    "Failed to parse server response into requested type");
            }
            return parse_result.value();
        }
    }
    /// @endcond

    /// @cond INTERNAL
    // Schema + client-side parser bundle. Every schema-backed generator
    // exposes one of these via IGenerator<T>::as_basic().
    template <typename T> struct BasicGenerator {
        using Parse =
            std::function<T(const hegel::internal::json::json_raw_ref&)>;

        hegel::internal::json::json schema;
        Parse parse;

        T parse_raw(const hegel::internal::json::json_raw_ref& raw) const {
            return parse(raw);
        }

        T do_draw(const TestCase& tc) const {
            hegel::internal::json::json response =
                internal::communicate_with_core(schema, tc);
            if (!response.contains("result")) {
                throw std::runtime_error(
                    "Server response missing 'result' field");
            }
            return parse(response["result"]);
        }

        template <typename F, typename U = std::invoke_result_t<F, T>>
        BasicGenerator<U> map(F f) const {
            Parse old_parse = parse;
            hegel::internal::json::json sch = schema;
            return BasicGenerator<U>{
                std::move(sch),
                [old_parse = std::move(old_parse), f = std::move(f)](
                    const hegel::internal::json::json_raw_ref& raw) -> U {
                    return f(old_parse(raw));
                }};
        }
    };
    /// @endcond

    /**
     * @brief Base interface for generators.
     *
     * IGenerator is not part of the public API. You should not implement it.
     *
     * @tparam T The type to generate values for
     */
    template <typename T> struct IGenerator {
        IGenerator() {}
        virtual ~IGenerator() = default;

        /// @cond INTERNAL
        // Returns a BasicGenerator<T> (schema + client-side parser) if this
        // generator can be driven through the Hegel protocol as a single
        // schema request. Composed generators (vectors, one_of, ...) use this
        // to build compound schemas while keeping per-element parsing
        // client-side; map() uses it to preserve schemas through
        // transformations. Defaults to nullopt for generators whose
        // production is fully client-side (filter, flat_map, user closures).
        virtual std::optional<BasicGenerator<T>> as_basic() const {
            return std::nullopt;
        }

        // Get the CBOR schema for this generator, if any. The default
        // derives it from as_basic(); override only if you need to report a
        // schema without also providing a parser.
        virtual std::optional<hegel::internal::json::json> schema() const {
            auto b = as_basic();
            return b ? std::optional{b->schema} : std::nullopt;
        }

        // Produce a value. The default delegates to the basic form when
        // available; generators without a basic form
        // must override this to provide a client-side fallback.
        virtual T do_draw(const TestCase& tc) const {
            if (auto b = as_basic())
                return b->do_draw(tc);
            throw std::logic_error(
                "IGenerator has no basic form and no do_draw override");
        }
        /// @endcond
    };

    /**
     * @brief The base class of all generators.
     *
     * @code{.cpp}
     * namespace gs = hegel::generators;
     *
     * hegel::test([](hegel::TestCase& tc) {
     *     // Create a generator and draw a value
     *     auto int_gen = gs::integers<int>({.min_value = 0, .max_value = 100});
     *     int value = tc.draw(int_gen);
     *
     *     // Transform with map
     *     auto squared = int_gen.map([](int x) { return x * x; });
     *
     *     // Filter values
     *     auto even = int_gen.filter([](int x) { return x % 2 == 0; });
     *
     *     // Dependent generation with flat_map
     *     auto sized = gs::integers<size_t>({.min_value = 1, .max_value = 10})
     *         .flat_map([](size_t len) {
     *             return gs::text({.min_size = len, .max_size = len});
     *         });
     * });
     * @endcode
     *
     * @tparam T The type to generate values for
     */
    template <typename T> class Generator : IGenerator<T> {
      public:
        /// @cond INTERNAL
        Generator(IGenerator<T>* p) : IGenerator<T>(), inner_(p) {}
        Generator(std::shared_ptr<IGenerator<T>> p)
            : IGenerator<T>(), inner_(std::move(p)) {}

        T do_draw(const TestCase& tc) const override {
            return inner_->do_draw(tc);
        }

        std::optional<hegel::internal::json::json> schema() const override {
            return inner_->schema();
        }

        std::optional<BasicGenerator<T>> as_basic() const override {
            return inner_->as_basic();
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
         * Here's an example of how you'd use this:
         * @code{.cpp}
         * Generator<double> halved =                           // Result type
         * Generator<double> gs::integers<int>() // Input type Generator<int>
         * .map(
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
        Generator<std::invoke_result_t<F, T>> map(F f) const {
            using ResultType = std::invoke_result_t<F, T>;
            return Generator<ResultType>(
                new MappedGenerator<T, ResultType>(inner_, std::move(f)));
        }

        /**
         * @brief Chain generators for dependent generation.
         *
         * Given a Generator&lt;T&gt; and a function from T ->
         * Generator&lt;S&gt;, creates a Generator&lt;S&gt;. Useful when
         * generation parameters depend on previously generated values.
         *
         * @code{.cpp}
         * Generator<std::string> sized_string =                     // Result
         * type Generator<std::string> gs::integers<size_t>({.min_value = 1,
         * .max_value = 10})   // Input type Generator<size_t> .flat_map(
         *         [](size_t len) {                                  //
         * transformation Generator<std::string> f(size_t len) return gs::text({
         * // gs::text() return type is Generator<std::string> .min_size = len,
         * // Constructor parameters to gs::text() depend on the value *len*
         * .max_size = len
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
                    std::declval<const TestCase&>()));
            auto inner = inner_;
            return compose([inner, f = std::forward<F>(f)](
                               const TestCase& tc) -> ResultType {
                return f(inner->do_draw(tc)).do_draw(tc);
            });
        }

        /**
         * @brief Filter generated values by a predicate.
         *
         * Creates a Generator that only produces values satisfying the
         * predicate. The new Generator has the same type as this Generator.
         *
         * So for example, if you want sorted lists of length N, you should
         * generate sorted lists of length N, not generate random lists and
         * filter by a predicate of 'length == N && is_sorted'. (Although the
         * latter is logically correct, it would be a performance nightmare, so
         * Hegel doesn't let you do it that way.)
         *
         * @code{.cpp}
         * Generator<int> even =                                  // Return type
         * is same as input type gs::integers<int>({.min_value = 0, .max_value =
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
            return compose([inner, pred](const TestCase& tc) -> T {
                for (int i = 0; i < 3; ++i) {
                    T value = inner->do_draw(tc);
                    if (pred(value)) {
                        return value;
                    }
                }
                tc.assume(false);
                // unreachable: assume(false) throws
                throw internal::HegelReject();
            });
        }

      private:
        std::shared_ptr<IGenerator<T>> inner_;
    };

    /// @cond INTERNAL
    // Generator that produces values by invoking a user-provided function.
    // Users never construct or reference this type directly; it's produced
    // internally by compose() and by map()/flat_map()/filter().
    template <typename T> class CompositeGenerator : public IGenerator<T> {
      public:
        explicit CompositeGenerator(std::function<T(const TestCase&)> fn)
            : gen_fn_(std::move(fn)) {}

        T do_draw(const TestCase& tc) const override { return gen_fn_(tc); }

      private:
        std::function<T(const TestCase&)> gen_fn_;
    };
    /// @endcond

    /// @cond INTERNAL
    // Generator that applies a client-side transformation to values drawn
    // from a source generator. Produced internally by Generator<T>::map().
    //
    // Preserves basic-ness (and therefore the server-side schema) by
    // composing the map function into the source's BasicGenerator::parse
    // step; falls back to `f(source->do_draw(tc))` when the source is not
    // basic.
    template <typename T, typename U>
    class MappedGenerator : public IGenerator<U> {
      public:
        MappedGenerator(std::shared_ptr<IGenerator<T>> source,
                        std::function<U(T)> f)
            : source_(std::move(source)), f_(std::move(f)) {}

        std::optional<BasicGenerator<U>> as_basic() const override {
            auto basic = source_->as_basic();
            if (!basic)
                return std::nullopt;
            return basic->map(f_);
        }

        U do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            return f_(source_->do_draw(tc));
        }

      private:
        std::shared_ptr<IGenerator<T>> source_;
        std::function<U(T)> f_;
    };
    /// @endcond

    /**
     * @brief Build a generator from imperative code that draws from a
     *        TestCase.
     *
     * The element type is deduced from @p fn's return type. To force a specific
     * type, give the lambda an explicit trailing return type.
     *
     * @code{.cpp}
     * auto generate_person() {
     *     return gs::compose([](const hegel::TestCase& tc) {
     *         int age = tc.draw(gs::integers<int>());
     *         std::string name = tc.draw(gs::text());
     *         return Person{age, name};
     *     });
     * }
     * @endcode
     *
     * @tparam F A callable taking `const TestCase&`
     * @param fn Function that draws from the TestCase and returns a value
     * @return A Generator whose element type is the return type of @p fn
     */
    template <typename F> auto compose(F&& fn) {
        using T = std::invoke_result_t<F, const TestCase&>;
        return Generator<T>(new CompositeGenerator<T>(
            std::function<T(const TestCase&)>(std::forward<F>(fn))));
    }

} // namespace hegel::generators

namespace hegel {

    template <typename T>
    T TestCase::draw(const generators::Generator<T>& gen) const {
        return gen.do_draw(*this);
    }

} // namespace hegel
