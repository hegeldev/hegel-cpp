#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "config.h"
#include "internal.h"
#include "json.h"
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
    // step is determined solely by T. Only primitives are parsed this way;
    // composites assemble their result from per-element parsers in their own
    // do_draw, so this is never instantiated for a non-primitive T.
    template <typename T>
    T default_parse_raw(const hegel::internal::json::json_raw_ref& result) {
        if constexpr (std::is_same_v<T, std::string>) {
            return result.get_string();
        } else if constexpr (std::is_same_v<
                                 std::remove_cv_t<std::remove_reference_t<T>>,
                                 bool>) {
            return result.get_bool();
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(result.get_double());
        } else if constexpr (std::is_unsigned_v<T>) {
            return static_cast<T>(result.get_uint64_t());
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(result.get_int64_t());
        } else {
            static_assert(
                internal::always_false_v<T>,
                "default_parse_raw only supports primitive types; provide an "
                "explicit generator/parser for T.");
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
                internal::generate_from_schema(schema, tc);
            if (!response.contains("result")) {
                // The engine always returns a "result"; a miss would be an
                // engine bug, not reachable from a test.
                // GCOVR_EXCL_START
                throw std::runtime_error(
                    "engine response missing 'result' field");
                // GCOVR_EXCL_STOP
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
        // Schema-backed generators build basic_ (schema + parser) in their
        // constructor; composites build theirs from their children's basic().
        // Generators with no schema path (filter, flat_map, user closures)
        // leave it empty and override do_draw().
        virtual const std::optional<BasicGenerator<T>>& basic() const {
            return basic_;
        }

        virtual std::optional<hegel::internal::json::json> schema() const {
            const auto& b = basic();
            return b ? std::optional{b->schema} : std::nullopt;
        }

        virtual T do_draw(const TestCase& tc) const {
            if (const auto& b = basic())
                return b->do_draw(tc);
            // GCOVR_EXCL_START
            throw std::logic_error(
                "IGenerator has no basic form and no do_draw override");
            // GCOVR_EXCL_STOP
        }
        /// @endcond

      protected:
        std::optional<BasicGenerator<T>> basic_;
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

        const std::optional<BasicGenerator<T>>& basic() const override {
            return inner_->basic();
        }
        /// @endcond

        /**
         * @brief Transform generated values with a function.
         *
         * Given a Generator\<T\> and a function T -> S, creates a
         * Generator\<S\>.
         *
         * This works by generating values from the Generator&lt;T&gt; and
         * applying a transformation to each value.
         *
         * Here's an example of how you'd use this:
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
        Generator<std::invoke_result_t<F, T>> map(F f) const {
            using ResultType = std::invoke_result_t<F, T>;
            return Generator<ResultType>(
                new MappedGenerator<T, ResultType>(inner_, std::move(f)));
        }

        /**
         * @brief Chain generators for dependent generation.
         *
         * Given a Generator\<T\> and a function T -> Generator\<S\>, creates a
         * Generator\<S\>. Useful when generation parameters depend on
         * previously generated values.
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
         * For example, if you want sorted lists of length N, you should
         * generate lists of length N and sort them, not generate random lists
         * and filter by a predicate of 'length == N && is_sorted'.
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
    // Preserves basic-ness (and therefore the engine-side schema) by
    // composing the map function into the source's BasicGenerator::parse
    // step; falls back to `f(source->do_draw(tc))` when the source is not
    // basic.
    template <typename T, typename U>
    class MappedGenerator : public IGenerator<U> {
      public:
        MappedGenerator(std::shared_ptr<IGenerator<T>> source,
                        std::function<U(T)> f)
            : source_(std::move(source)), f_(std::move(f)) {
            if (const auto& b = source_->basic()) {
                this->basic_.emplace(b->map(f_));
            }
        }

        U do_draw(const TestCase& tc) const override {
            if (const auto& basic = this->basic()) {
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
