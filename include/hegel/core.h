#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "config.h"
#include "internal.h"
#include "test_case.h"

/**
 * @brief Hegel generators.
 */
namespace hegel::generators {

    template <typename T> class CompositeGenerator;
    template <typename T, typename U> class MappedGenerator;

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
        // Every generator produces its value by driving the engine's typed
        // draw primitives (hegel::internal::draw_*) against the test case.
        virtual T do_draw(const TestCase& tc) const = 0;
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
    template <typename T> class Generator {
      public:
        /// The type of value this generator produces.
        using value_type = T;

        /// @cond INTERNAL
        Generator(IGenerator<T>* p) : inner_(p) {}
        Generator(std::shared_ptr<IGenerator<T>> p) : inner_(std::move(p)) {}

        T do_draw(const TestCase& tc) const { return inner_->do_draw(tc); }
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
        Generator<std::invoke_result_t<F, T>> map(F&& f) const {
            using ResultType = std::invoke_result_t<F, T>;
            return Generator<ResultType>(
                new MappedGenerator<T, ResultType>(inner_, std::forward<F>(f)));
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
            // F: T -> Generator<ResultType>; flat_map returns the same
            // Generator<ResultType>.
            using ResultType = typename std::invoke_result_t<F, T>::value_type;
            auto inner = inner_;
            return compose([inner, f = std::forward<F>(f)](
                               const TestCase& tc) -> ResultType {
                internal::start_span(tc, internal::SpanLabel::FlatMap);
                ResultType result = f(inner->do_draw(tc)).do_draw(tc);
                internal::stop_span(tc);
                return result;
            });
        }

        /**
         * @brief Filter generated values by a predicate.
         *
         * Creates a Generator that only produces values satisfying the
         * predicate. The new Generator has the same type as this Generator.
         *
         * Each draw makes up to three attempts to generate a value that
         * satisfies @p pred (rejected attempts are discarded so they don't
         * pollute shrinking). If all three attempts fail, the *whole test
         * case* is rejected — as if `tc.assume(false)` had been called — and
         * the engine moves on to a fresh one. Rejected test cases don't count
         * toward Settings::test_cases, and a predicate that rejects too often
         * fails the HealthCheck::FilterTooMuch health check.
         *
         * This makes filter suitable only for predicates that accept most
         * values. Prefer constructing the values you want over filtering
         * out the ones you don't: for sorted lists of length N, generate
         * lists of length N and sort them, rather than filtering arbitrary
         * lists on `length == N && is_sorted` (which would reject almost
         * everything).
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
                    internal::start_span(tc, internal::SpanLabel::Filter);
                    T value = inner->do_draw(tc);
                    if (pred(value)) {
                        internal::stop_span(tc);
                        return value;
                    }
                    // Discard the rejected span so the engine retries from
                    // before it opened.
                    internal::stop_span(tc, true);
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
    template <typename T, typename U>
    class MappedGenerator : public IGenerator<U> {
      public:
        MappedGenerator(std::shared_ptr<IGenerator<T>> source,
                        std::function<U(T)> f)
            : source_(std::move(source)), f_(std::move(f)) {}

        U do_draw(const TestCase& tc) const override {
            internal::start_span(tc, internal::SpanLabel::Mapped);
            U result = f_(source_->do_draw(tc));
            internal::stop_span(tc);
            return result;
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
