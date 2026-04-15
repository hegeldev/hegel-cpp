#pragma once

/**
 * @file combinators.h
 * @brief Combinator generator functions: sampled_from, one_of, variant_,
 * optional_
 */

#include <variant>

#include "hegel/core.h"
#include "hegel/generators/numeric.h"
#include "hegel/generators/primitives.h"

namespace hegel::generators {

    /// @name Combinator Strategies
    /// @{

    /**
     * @brief Sample uniformly from a fixed set of values.
     *
     * @code{.cpp}
     * auto color = sampled_from({"red", "green", "blue"});
     * auto digit = sampled_from({1, 2, 3, 4, 5});
     * @endcode
     *
     * @tparam T Element type
     * @param elements Vector of values to sample from (must not be empty)
     * @return Generator that picks uniformly from elements
     */
    template <typename T>
    Generator<T> sampled_from(const std::vector<T>& elements) {
        if (elements.empty()) {
            throw std::invalid_argument(
                "sampled_from requires a non-empty vector");
        }

        if constexpr (std::is_same_v<T, bool> ||
                      std::is_same_v<T, std::nullptr_t> ||
                      std::is_integral_v<T> || std::is_floating_point_v<T> ||
                      std::is_same_v<T, std::string>) {
            hegel::internal::json::json arr =
                hegel::internal::json::json::array();
            for (const auto& e : elements)
                arr.push_back(e);
            hegel::internal::json::json schema = {{"type", "sampled_from"},
                                                  {"values", arr}};
            return from_schema<T>(std::move(schema));
        } else {
            auto index_gen = integers<size_t>(
                {.min_value = 0, .max_value = elements.size() - 1});

            return from_function<T>([elements, index_gen](TestCaseData* data) {
                size_t idx = index_gen.do_draw(data);
                return elements[idx];
            });
        }
    }

    /// @overload sampled_from(std::initializer_list<T>)
    template <typename T>
    Generator<T> sampled_from(std::initializer_list<T> elements) {
        return sampled_from(std::vector<T>(elements));
    }

    /// @overload
    inline Generator<std::string>
    sampled_from(std::initializer_list<const char*> elements) {
        std::vector<std::string> strings;
        strings.reserve(elements.size());
        for (const char* s : elements) {
            strings.push_back(s);
        }
        return sampled_from(strings);
    }

    /// @cond INTERNAL
    namespace detail {

        template <typename T>
        auto make_tagged_one_of(std::vector<Generator<T>> gens)
            -> Generator<T> {
            using json = hegel::internal::json::json;
            using json_raw_ref = hegel::internal::json::json_raw_ref;

            std::vector<std::function<T(json_raw_ref)>> appliers;
            json tagged_schemas = json::array();

            for (size_t i = 0; i < gens.size(); ++i) {
                appliers.push_back(*gens[i].get_applier());

                json elements = json::array();
                elements.push_back(
                    json({{"type", "constant"}, {"value", (int64_t)i}}));
                elements.push_back(*gens[i].schema());
                json tuple_schema = {{"type", "tuple"}, {"elements", elements}};
                tagged_schemas.push_back(std::move(tuple_schema));
            }

            json one_of_schema = {{"type", "one_of"},
                                  {"generators", tagged_schemas}};

            return from_function<T>(
                [appliers, one_of_schema](TestCaseData* data) -> T {
                    json response =
                        internal::communicate_with_core(one_of_schema, data);
                    if (!response.contains("result")) {
                        throw std::runtime_error(
                            "Server response missing 'result' field");
                    }
                    json_raw_ref result = response["result"];
                    auto idx = static_cast<size_t>(result[0].get_int64_t());
                    return appliers[idx](result[1]);
                },
                one_of_schema);
        }

        template <typename T>
        auto make_one_of_schema(const std::vector<Generator<T>>& gens)
            -> std::optional<hegel::internal::json::json> {
            for (const auto& gen : gens) {
                if (!gen.schema() || gen.has_client_transform())
                    return std::nullopt;
            }

            hegel::internal::json::json one_of_arr =
                hegel::internal::json::json::array();
            for (const auto& gen : gens) {
                one_of_arr.push_back(*gen.schema());
            }

            hegel::internal::json::json schema = {{"type", "one_of"},
                                                  {"generators", one_of_arr}};
            return schema;
        }

    } // namespace detail
    /// @endcond

    /**
     * @brief Choose from multiple generators of the same type.
     *
     * Randomly selects one generator and uses it to produce a value.
     *
     * Uses a single server round-trip whenever all generators are
     * schema-backed (with or without a client-side transform via map()).
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
        if (gens.empty()) {
            throw std::invalid_argument(
                "one_of requires a non-empty vector of generators");
        }

        // Path 1: all basic (schema, no client transform) → simple one_of
        auto maybe_schema = detail::make_one_of_schema(gens);
        if (maybe_schema) {
            return from_schema<T>(std::move(*maybe_schema));
        }

        // Path 2: all have schema + applier, some with client transform →
        // tagged-tuple schema for a single server round-trip
        bool all_have_appliers = true;
        for (const auto& gen : gens) {
            if (!gen.schema() || !gen.get_applier()) {
                all_have_appliers = false;
                break;
            }
        }
        if (all_have_appliers) {
            return detail::make_tagged_one_of(std::move(gens));
        }

        // Path 3: at least one generator lacks a schema → function-based
        auto index_gen =
            integers<size_t>({.min_value = 0, .max_value = gens.size() - 1});

        return from_function<T>([gens, index_gen](TestCaseData* data) {
            size_t idx = index_gen.do_draw(data);
            return gens[idx].do_draw(data);
        });
    }

    /// @overload one_of(std::initializer_list<Generator<T>>)
    template <typename T>
    Generator<T> one_of(std::initializer_list<Generator<T>> gens) {
        return one_of(std::vector<Generator<T>>(gens));
    }

    /// @cond INTERNAL
    namespace detail {

        template <typename Variant, typename GenTuple, size_t I = 0>
        Variant draw_variant_impl(const GenTuple& gens, size_t idx,
                                  TestCaseData* data) {
            if constexpr (I < std::tuple_size_v<GenTuple>) {
                if (idx == I) {
                    return std::get<I>(gens).do_draw(data);
                }
                return draw_variant_impl<Variant, GenTuple, I + 1>(gens, idx,
                                                                   data);
            } else {
                return Variant{};
            }
        }

    } // namespace detail
    /// @endcond

    /**
     * @brief Generate std::variant from heterogeneous generators.
     *
     * Each generator produces one possible variant alternative.
     *
     * @code{.cpp}
     * auto value = variant_(integers<int>(), text(), booleans());
     * // Returns std::variant<int, std::string, bool>
     * @endcode
     *
     * @tparam Ts Variant alternative types
     * @param gens Generators for each alternative
     * @return Generator producing variants
     */
    template <typename... Ts>
    Generator<std::variant<Ts...>> variant_(Generator<Ts>... gens) {
        using ResultVariant = std::variant<Ts...>;
        constexpr size_t N = sizeof...(Ts);

        auto gen_tuple = std::make_tuple(std::move(gens)...);
        auto index_gen = integers<size_t>({.min_value = 0, .max_value = N - 1});

        return from_function<ResultVariant>([gen_tuple,
                                             index_gen](TestCaseData* data) {
            size_t idx = index_gen.do_draw(data);
            return detail::draw_variant_impl<ResultVariant,
                                             decltype(gen_tuple)>(gen_tuple,
                                                                  idx, data);
        });
    }

    /**
     * @brief Generate optional values (present or absent).
     *
     * Randomly produces either a value from the given generator or
     * std::nullopt.
     *
     * @code{.cpp}
     * auto maybe_int = optional_(integers<int>());
     * // Returns std::optional<int>, may be nullopt
     * @endcode
     *
     * @tparam T Value type
     * @param gen Generator for the value when present
     * @return Generator producing optional values
     */
    template <typename T>
    Generator<std::optional<T>> optional_(Generator<T> gen) {
        auto bool_gen = booleans();

        return from_function<std::optional<T>>(
            [gen, bool_gen](TestCaseData* data) -> std::optional<T> {
                bool is_none = bool_gen.do_draw(data);
                if (is_none) {
                    return std::nullopt;
                }
                return gen.do_draw(data);
            });
    }

    /// @}

} // namespace hegel::generators
