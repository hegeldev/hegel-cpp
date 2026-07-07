#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "hegel/core.h"

namespace hegel::generators {

    /// @cond INTERNAL
    // Concrete IGenerator for sampled_from(). Draws an index into the
    // captured `elements_` vector and returns that element.
    template <typename T> class SampledFromGenerator : public IGenerator<T> {
      public:
        explicit SampledFromGenerator(std::vector<T> elements)
            : elements_(std::move(elements)) {
            if (elements_.empty()) {
                throw std::invalid_argument(
                    "sampled_from requires a non-empty vector");
            }
        }

        T do_draw(const TestCase& tc) const override {
            int64_t index = hegel::internal::draw_integer(
                tc, 0, static_cast<int64_t>(elements_.size() - 1));
            return elements_[static_cast<size_t>(index)];
        }

      private:
        std::vector<T> elements_;
    };

    // Concrete IGenerator for one_of(). Draws a branch index inside a
    // one_of span, then delegates to that branch.
    template <typename T> class OneOfGenerator : public IGenerator<T> {
      public:
        explicit OneOfGenerator(std::vector<Generator<T>> gens)
            : gens_(std::move(gens)) {
            if (gens_.empty()) {
                throw std::invalid_argument(
                    "one_of requires a non-empty vector of generators");
            }
        }

        T do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            hi::start_span(tc, hi::SpanLabel::OneOf);
            int64_t index =
                hi::draw_integer(tc, 0, static_cast<int64_t>(gens_.size() - 1));
            T result = gens_[static_cast<size_t>(index)].do_draw(tc);
            hi::stop_span(tc);
            return result;
        }

      private:
        std::vector<Generator<T>> gens_;
    };
    /// @endcond

    /// @name Misc
    /// @{

    /**
     * @brief Sample from a fixed set of values.
     *
     * @code{.cpp}
     * auto color = sampled_from({"red", "green", "blue"});
     * auto digit = sampled_from({1, 2, 3, 4, 5});
     * @endcode
     *
     * @tparam T Element type
     * @param elements Vector of values to sample from (must not be empty)
     * @return Generator that picks from elements
     */
    template <typename T>
    Generator<T> sampled_from(const std::vector<T>& elements) {
        return Generator<T>(new SampledFromGenerator<T>(elements));
    }

    /**
     * @brief Sample from a fixed set of values (initializer list).
     * @tparam T Element type
     * @param elements Values to sample from (must not be empty)
     * @return Generator that picks from elements
     */
    template <typename T>
    Generator<T> sampled_from(std::initializer_list<T> elements) {
        return sampled_from(std::vector<T>(elements));
    }

    /**
     * @brief Sample from a fixed set of C-string literals.
     * @param elements String literals to sample from (must not be empty)
     * @return Generator of std::string picking from elements
     */
    inline Generator<std::string>
    sampled_from(std::initializer_list<const char*> elements) {
        std::vector<std::string> strings;
        strings.reserve(elements.size());
        for (const char* s : elements) {
            strings.push_back(s);
        }
        return sampled_from(strings);
    }

    /// @}

    /// @name Combinators
    /// @{

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
    template <typename T> Generator<T> one_of(std::vector<Generator<T>> gens) {
        return Generator<T>(new OneOfGenerator<T>(std::move(gens)));
    }

    /**
     * @brief Choose from a list of generators (initializer list).
     * @tparam T Value type produced by each generator
     * @param gens Generators to choose from (must not be empty)
     * @return Generator that picks from gens and forwards do_draw
     */
    template <typename T>
    Generator<T> one_of(std::initializer_list<Generator<T>> gens) {
        return one_of(std::vector<Generator<T>>(gens));
    }

    /// @cond INTERNAL
    namespace detail {

        template <typename Variant, typename GenTuple, size_t I = 0>
        Variant draw_variant_impl(const GenTuple& gens, size_t idx,
                                  const TestCase& tc) {
            if constexpr (I < std::tuple_size_v<GenTuple>) {
                if (idx == I) {
                    return Variant{std::in_place_index<I>,
                                   std::get<I>(gens).do_draw(tc)};
                }
                return draw_variant_impl<Variant, GenTuple, I + 1>(gens, idx,
                                                                   tc);
            } else {
                // Unreachable: idx is always in [0, N), so an earlier branch
                // matches before the recursion bottoms out.
                return Variant{}; // GCOVR_EXCL_LINE
            }
        }

    } // namespace detail

    // Concrete IGenerator for variant(). Draws a branch index inside a
    // one_of span; branches can have heterogeneous types.
    template <typename... Ts>
    class VariantGenerator : public IGenerator<std::variant<Ts...>> {
      public:
        using Result = std::variant<Ts...>;

        explicit VariantGenerator(Generator<Ts>... gens)
            : gens_(std::move(gens)...) {}

        Result do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            constexpr size_t N = sizeof...(Ts);
            hi::start_span(tc, hi::SpanLabel::OneOf);
            int64_t index =
                hi::draw_integer(tc, 0, static_cast<int64_t>(N - 1));
            Result result = detail::draw_variant_impl<Result, decltype(gens_)>(
                gens_, static_cast<size_t>(index), tc);
            hi::stop_span(tc);
            return result;
        }

      private:
        std::tuple<Generator<Ts>...> gens_;
    };

    // Concrete IGenerator for optional(). A boolean draw inside an
    // optional span gates presence; false shrinks toward nullopt.
    template <typename T>
    class OptionalGenerator : public IGenerator<std::optional<T>> {
      public:
        explicit OptionalGenerator(Generator<T> gen) : gen_(std::move(gen)) {}

        std::optional<T> do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            hi::start_span(tc, hi::SpanLabel::Optional);
            std::optional<T> result;
            if (hi::draw_boolean(tc, 0.5)) {
                result = gen_.do_draw(tc);
            }
            hi::stop_span(tc);
            return result;
        }

      private:
        Generator<T> gen_;
    };
    /// @endcond

    /**
     * @brief Generate std::variant from heterogeneous generators.
     *
     * Each generator produces one possible variant alternative.
     *
     * @code{.cpp}
     * auto value = variant(integers<int>(), text(), booleans());
     * // Returns std::variant<int, std::string, bool>
     * @endcode
     *
     * @tparam Ts Variant alternative types
     * @param gens Generators for each alternative
     * @return Generator producing variant
     */
    template <typename... Ts>
    Generator<std::variant<Ts...>> variant(Generator<Ts>... gens) {
        return Generator<std::variant<Ts...>>(
            new VariantGenerator<Ts...>(std::move(gens)...));
    }

    /**
     * @brief Generate optional values (present or absent).
     *
     * Randomly produces either a value from the given generator or
     * std::nullopt.
     *
     * @code{.cpp}
     * auto maybe_int = optional(integers<int>());
     * // Returns std::optional<int>, may be nullopt
     * @endcode
     *
     * @tparam T Value type
     * @param gen Generator for the value when present
     * @return Generator producing optional values
     */
    template <typename T>
    Generator<std::optional<T>> optional(Generator<T> gen) {
        return Generator<std::optional<T>>(
            new OptionalGenerator<T>(std::move(gen)));
    }

    /// @}

} // namespace hegel::generators
