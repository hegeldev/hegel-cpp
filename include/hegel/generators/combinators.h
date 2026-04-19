#pragma once

#include <variant>

#include "hegel/core.h"
#include "hegel/generators/numeric.h"
#include "hegel/generators/primitives.h"

namespace hegel::generators {

    /// @cond INTERNAL
    // Concrete IGenerator for sampled_from(). Schema asks the server for an
    // integer index into the captured `elements_` vector; the client parser
    // does the lookup.
    template <typename T> class SampledFromGenerator : public IGenerator<T> {
      public:
        explicit SampledFromGenerator(std::vector<T> elements)
            : elements_(std::move(elements)) {
            if (elements_.empty()) {
                throw std::invalid_argument(
                    "sampled_from requires a non-empty vector");
            }
        }

        std::optional<BasicGenerator<T>> as_basic() const override {
            hegel::internal::json::json schema = {
                {"type", "integer"},
                {"min_value", 0},
                {"max_value", static_cast<int64_t>(elements_.size() - 1)}};
            auto elements = elements_;
            return BasicGenerator<T>{
                std::move(schema),
                [elements = std::move(elements)](
                    const hegel::internal::json::json_raw_ref& raw) {
                    return elements[static_cast<size_t>(raw.get_int64_t())];
                }};
        }

      private:
        std::vector<T> elements_;
    };

    // Concrete IGenerator for one_of(). Schema path requires every branch
    // to be basic; fallback draws an index and delegates to that branch.
    template <typename T> class OneOfGenerator : public IGenerator<T> {
      public:
        explicit OneOfGenerator(std::vector<Generator<T>> gens)
            : gens_(std::move(gens)) {
            if (gens_.empty()) {
                throw std::invalid_argument(
                    "one_of requires a non-empty vector of generators");
            }
        }

        std::optional<BasicGenerator<T>> as_basic() const override {
            std::vector<BasicGenerator<T>> basics;
            basics.reserve(gens_.size());
            for (const auto& gen : gens_) {
                auto b = gen.as_basic();
                if (!b)
                    return std::nullopt;
                basics.push_back(std::move(*b));
            }

            // Tag each branch with its index so the client knows which
            // parser to apply. Schema per branch is [constant(i), value].
            hegel::internal::json::json tagged =
                hegel::internal::json::json::array();
            for (size_t i = 0; i < basics.size(); ++i) {
                hegel::internal::json::json elements =
                    hegel::internal::json::json::array();
                elements.push_back(hegel::internal::json::json{
                    {"type", "constant"}, {"value", (int64_t)i}});
                elements.push_back(basics[i].schema);
                tagged.push_back(hegel::internal::json::json{
                    {"type", "tuple"}, {"elements", elements}});
            }
            hegel::internal::json::json schema = {{"type", "one_of"},
                                                  {"generators", tagged}};

            return BasicGenerator<T>{
                std::move(schema),
                [basics = std::move(basics)](
                    const hegel::internal::json::json_raw_ref& raw) -> T {
                    size_t idx = static_cast<size_t>(raw[0].get_int64_t());
                    return basics[idx].parse_raw(raw[1]);
                }};
        }

        T do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            auto idx = integers<size_t>(
                           {.min_value = 0, .max_value = gens_.size() - 1})
                           .do_draw(tc);
            return gens_[idx].do_draw(tc);
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
                    return std::get<I>(gens).do_draw(tc);
                }
                return draw_variant_impl<Variant, GenTuple, I + 1>(gens, idx,
                                                                   tc);
            } else {
                return Variant{};
            }
        }

        template <typename Variant, typename Parsers, size_t I = 0>
        Variant
        parse_variant_impl(const Parsers& parsers, size_t idx,
                           const hegel::internal::json::json_raw_ref& raw) {
            if constexpr (I < std::tuple_size_v<Parsers>) {
                if (idx == I) {
                    return Variant{std::in_place_index<I>,
                                   std::get<I>(parsers)(raw)};
                }
                return parse_variant_impl<Variant, Parsers, I + 1>(parsers, idx,
                                                                   raw);
            } else {
                return Variant{};
            }
        }

    } // namespace detail

    // Concrete IGenerator for variant(). Schema path requires every branch
    // to be basic and uses a tagged one_of (same wire format as
    // OneOfGenerator, but branches can have heterogeneous types).
    template <typename... Ts>
    class VariantGenerator : public IGenerator<std::variant<Ts...>> {
      public:
        using Result = std::variant<Ts...>;

        explicit VariantGenerator(Generator<Ts>... gens)
            : gens_(std::move(gens)...) {}

        std::optional<BasicGenerator<Result>> as_basic() const override {
            auto basics = std::apply(
                [](const auto&... g) {
                    return std::make_tuple(g.as_basic()...);
                },
                gens_);
            bool all_basic = std::apply(
                [](const auto&... b) { return (b.has_value() && ...); },
                basics);
            if (!all_basic)
                return std::nullopt;

            hegel::internal::json::json tagged =
                hegel::internal::json::json::array();
            size_t i = 0;
            std::apply(
                [&tagged, &i](const auto&... b) {
                    ((tagged.push_back(hegel::internal::json::json{
                          {"type", "tuple"},
                          {"elements", hegel::internal::json::json::array(
                                           {hegel::internal::json::json{
                                                {"type", "constant"},
                                                {"value", (int64_t)i}},
                                            b->schema})}}),
                      ++i),
                     ...);
                },
                basics);

            hegel::internal::json::json schema = {{"type", "one_of"},
                                                  {"generators", tagged}};

            auto parsers = std::apply(
                [](const auto&... b) { return std::make_tuple(b->parse...); },
                basics);

            return BasicGenerator<Result>{
                std::move(schema),
                [parsers = std::move(parsers)](
                    const hegel::internal::json::json_raw_ref& raw) -> Result {
                    size_t idx = static_cast<size_t>(raw[0].get_int64_t());
                    return detail::parse_variant_impl<Result,
                                                      decltype(parsers)>(
                        parsers, idx, raw[1]);
                }};
        }

        Result do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            constexpr size_t N = sizeof...(Ts);
            auto index_gen =
                integers<size_t>({.min_value = 0, .max_value = N - 1});
            size_t idx = index_gen.do_draw(tc);
            return detail::draw_variant_impl<Result, decltype(gens_)>(gens_,
                                                                      idx, tc);
        }

      private:
        std::tuple<Generator<Ts>...> gens_;
    };

    // Concrete IGenerator for optional(). Schema path wraps the inner's
    // schema in a one_of with a null branch; fallback uses booleans() to
    // gate presence.
    template <typename T>
    class OptionalGenerator : public IGenerator<std::optional<T>> {
      public:
        explicit OptionalGenerator(Generator<T> gen) : gen_(std::move(gen)) {}

        std::optional<BasicGenerator<std::optional<T>>>
        as_basic() const override {
            auto basic = gen_.as_basic();
            if (!basic)
                return std::nullopt;

            hegel::internal::json::json generators =
                hegel::internal::json::json::array();
            generators.push_back(hegel::internal::json::json{{"type", "null"}});
            generators.push_back(basic->schema);
            hegel::internal::json::json schema = {{"type", "one_of"},
                                                  {"generators", generators}};

            auto parse = basic->parse;
            return BasicGenerator<std::optional<T>>{
                std::move(schema),
                [parse = std::move(parse)](
                    const hegel::internal::json::json_raw_ref& raw)
                    -> std::optional<T> {
                    if (raw.is_null()) {
                        return std::nullopt;
                    }
                    return parse(raw);
                }};
        }

        std::optional<T> do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            bool is_none = booleans().do_draw(tc);
            if (is_none) {
                return std::nullopt;
            }
            return gen_.do_draw(tc);
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
