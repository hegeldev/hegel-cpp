#pragma once

#include <map>
#include <set>
#include <tuple>
#include <vector>

#include "hegel/core.h"
#include "hegel/generators/numeric.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for vectors() generator.
     */
    struct VectorsParams {
        size_t min_size = 0;            ///< Minimum vector size
        std::optional<size_t> max_size; ///< Maximum vector size. Default: 100
        bool unique = false; ///< If true, all elements must be unique
    };

    /**
     * @brief Parameters for sets() generator.
     */
    struct SetsParams {
        size_t min_size = 0;            ///< Minimum set size
        std::optional<size_t> max_size; ///< Maximum set size. Default: 20
    };

    /**
     * @brief Parameters for maps() generator.
     */
    struct MapsParams {
        size_t min_size = 0; ///< Minimum number of entries
        std::optional<size_t>
            max_size; ///< Maximum number of entries. Default: 20
    };

    /// @cond INTERNAL
    // Concrete IGenerator for vectors(). Schema path when the element
    // generator is basic; otherwise a client-side length draw + element
    // draws loop.
    template <typename T>
    class VectorsGenerator : public IGenerator<std::vector<T>> {
      public:
        VectorsGenerator(Generator<T> elements, VectorsParams params = {})
            : elements_(std::move(elements)), params_(params) {
            if (params_.max_size && params_.min_size > *params_.max_size) {
                throw std::invalid_argument("Cannot have max_size < min_size");
            }
        }

        std::optional<BasicGenerator<std::vector<T>>>
        as_basic() const override {
            auto basic = elements_.as_basic();
            if (!basic)
                return std::nullopt;

            hegel::internal::json::json schema = {
                {"type", "list"},
                {"elements", basic->schema},
                {"min_size", params_.min_size},
                {"unique", params_.unique}};
            if (params_.max_size)
                schema["max_size"] = *params_.max_size;

            auto parse = basic->parse;
            return BasicGenerator<std::vector<T>>{
                std::move(schema),
                [parse = std::move(parse)](
                    const hegel::internal::json::json_raw_ref& raw)
                    -> std::vector<T> {
                    std::vector<T> result;
                    auto items = raw.iterate();
                    result.reserve(items.size());
                    for (const auto& item : items) {
                        result.push_back(parse(item));
                    }
                    return result;
                }};
        }

        std::vector<T> do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            size_t max_size = params_.max_size.value_or(100);
            auto length_gen = integers<size_t>(
                {.min_value = params_.min_size, .max_value = max_size});
            size_t len = length_gen.do_draw(tc);
            std::vector<T> result;
            result.reserve(len);
            for (size_t i = 0; i < len; ++i) {
                result.push_back(elements_.do_draw(tc));
            }
            return result;
        }

      private:
        Generator<T> elements_;
        VectorsParams params_;
    };

    // Concrete IGenerator for sets(). Schema path uses the list schema with
    // unique=true; fallback uniques are enforced by std::set semantics.
    template <typename T> class SetsGenerator : public IGenerator<std::set<T>> {
      public:
        SetsGenerator(Generator<T> elements, SetsParams params = {})
            : elements_(std::move(elements)), params_(params) {
            if (params_.max_size && params_.min_size > *params_.max_size) {
                throw std::invalid_argument("Cannot have max_size < min_size");
            }
        }

        std::optional<BasicGenerator<std::set<T>>> as_basic() const override {
            auto basic = elements_.as_basic();
            if (!basic)
                return std::nullopt;

            hegel::internal::json::json schema = {
                {"type", "list"},
                {"elements", basic->schema},
                {"min_size", params_.min_size},
                {"unique", true}};
            if (params_.max_size)
                schema["max_size"] = *params_.max_size;

            auto parse = basic->parse;
            return BasicGenerator<std::set<T>>{
                std::move(schema),
                [parse = std::move(parse)](
                    const hegel::internal::json::json_raw_ref& raw)
                    -> std::set<T> {
                    std::set<T> result;
                    for (const auto& item : raw.iterate()) {
                        result.insert(parse(item));
                    }
                    return result;
                }};
        }

        std::set<T> do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            size_t max_size = params_.max_size.value_or(20);
            auto length_gen = integers<size_t>(
                {.min_value = params_.min_size, .max_value = max_size});
            size_t target_len = length_gen.do_draw(tc);
            std::set<T> result;
            size_t attempts = 0;
            while (result.size() < target_len && attempts < target_len * 10) {
                result.insert(elements_.do_draw(tc));
                ++attempts;
            }
            return result;
        }

      private:
        Generator<T> elements_;
        SetsParams params_;
    };

    // Concrete IGenerator for maps(). Schema path requires both keys and
    // values to be basic; fallback draws K/V and inserts until `len`
    // unique keys are produced.
    template <typename K, typename V>
    class MapsGenerator : public IGenerator<std::map<K, V>> {
      public:
        MapsGenerator(Generator<K> keys, Generator<V> values,
                      MapsParams params = {})
            : keys_(std::move(keys)), values_(std::move(values)),
              params_(params) {
            if (params_.max_size && params_.min_size > *params_.max_size) {
                throw std::invalid_argument("Cannot have max_size < min_size");
            }
        }

        std::optional<BasicGenerator<std::map<K, V>>>
        as_basic() const override {
            auto k_basic = keys_.as_basic();
            auto v_basic = values_.as_basic();
            if (!k_basic || !v_basic)
                return std::nullopt;

            hegel::internal::json::json schema = {
                {"type", "dict"},
                {"keys", k_basic->schema},
                {"values", v_basic->schema},
                {"min_size", params_.min_size}};
            if (params_.max_size)
                schema["max_size"] = *params_.max_size;

            auto kp = k_basic->parse;
            auto vp = v_basic->parse;
            // Wire format is [[key, value], ...]
            return BasicGenerator<std::map<K, V>>{
                std::move(schema),
                [kp = std::move(kp), vp = std::move(vp)](
                    const hegel::internal::json::json_raw_ref& raw)
                    -> std::map<K, V> {
                    std::map<K, V> result;
                    for (const auto& pair : raw.iterate()) {
                        result.emplace(kp(pair[0]), vp(pair[1]));
                    }
                    return result;
                }};
        }

        std::map<K, V> do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            size_t max_size = params_.max_size.value_or(20);
            auto length_gen = integers<size_t>(
                {.min_value = params_.min_size, .max_value = max_size});
            size_t len = length_gen.do_draw(tc);
            std::map<K, V> result;
            while (result.size() < len) {
                K key = keys_.do_draw(tc);
                V value = values_.do_draw(tc);
                result[std::move(key)] = std::move(value);
            }
            return result;
        }

      private:
        Generator<K> keys_;
        Generator<V> values_;
        MapsParams params_;
    };
    /// @endcond

    /// @name Collections
    /// @{

    /**
     * @brief Generate vectors with elements from another generator.
     *
     * @code{.cpp}
     * auto int_vec = vectors(integers<int>());
     * auto bounded = vectors(integers<int>(), {.min_size = 1, .max_size = 10});
     * auto unique_vec = vectors(integers<int>(), {.unique = true});
     * @endcode
     *
     * @tparam T Element type
     * @param elements Generator for vector elements
     * @param params Size and uniqueness constraints
     * @return Generator producing vectors
     */
    template <typename T>
    Generator<std::vector<T>> vectors(Generator<T> elements,
                                      VectorsParams params = {}) {
        return Generator<std::vector<T>>(
            new VectorsGenerator<T>(std::move(elements), params));
    }

    /**
     * @brief Generate sets with elements from another generator.
     *
     * @code{.cpp}
     * auto int_set = sets(integers<int>());
     * auto bounded = sets(integers<int>(), {.min_size = 1, .max_size = 5});
     * @endcode
     *
     * @tparam T Element type (must be comparable)
     * @param elements Generator for set elements
     * @param params Size constraints
     * @return Generator producing sets
     */
    template <typename T>
    Generator<std::set<T>> sets(Generator<T> elements, SetsParams params = {}) {
        return Generator<std::set<T>>(
            new SetsGenerator<T>(std::move(elements), params));
    }

    /**
     * @brief Generate maps with configurable key and value types.
     *
     * @code{.cpp}
     * // String keys
     * auto strMap = maps(text(), integers<int>());
     *
     * // Integer keys
     * auto intMap = maps(integers<int>(), text());
     *
     * // With size bounds
     * auto bounded = maps(text(), integers<int>(), {.min_size = 1,
     * .max_size = 3});
     * @endcode
     *
     * @tparam K Key type
     * @tparam V Value type
     * @param keys Generator for map keys
     * @param values Generator for map values
     * @param params Size constraints
     * @return Generator producing maps
     */
    template <typename K, typename V>
    Generator<std::map<K, V>> maps(Generator<K> keys, Generator<V> values,
                                   MapsParams params = {}) {
        return Generator<std::map<K, V>>(new MapsGenerator<K, V>(
            std::move(keys), std::move(values), params));
    }

    /// @cond INTERNAL
    namespace detail {

        template <typename Tuple, typename GenTuple, size_t... Is>
        Tuple draw_tuple_impl(const GenTuple& gens, std::index_sequence<Is...>,
                              const TestCase& tc) {
            return Tuple{std::get<Is>(gens).do_draw(tc)...};
        }

    } // namespace detail

    // Concrete IGenerator for tuples(). Schema path requires every element
    // generator to be basic.
    template <typename... Ts>
    class TuplesGenerator : public IGenerator<std::tuple<Ts...>> {
      public:
        using ResultTuple = std::tuple<Ts...>;

        explicit TuplesGenerator(Generator<Ts>... gens)
            : gens_(std::move(gens)...) {}

        std::optional<BasicGenerator<ResultTuple>> as_basic() const override {
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

            hegel::internal::json::json elements =
                hegel::internal::json::json::array();
            std::apply(
                [&elements](const auto&... b) {
                    (elements.push_back(b->schema), ...);
                },
                basics);

            hegel::internal::json::json schema = {{"type", "tuple"},
                                                  {"elements", elements}};

            auto parsers = std::apply(
                [](const auto&... b) { return std::make_tuple(b->parse...); },
                basics);

            return BasicGenerator<ResultTuple>{
                std::move(schema),
                [parsers = std::move(parsers)](
                    const hegel::internal::json::json_raw_ref& raw)
                    -> ResultTuple {
                    return [&]<size_t... Is>(std::index_sequence<Is...>) {
                        return ResultTuple{std::get<Is>(parsers)(raw[Is])...};
                    }(std::index_sequence_for<Ts...>{});
                }};
        }

        ResultTuple do_draw(const TestCase& tc) const override {
            if (auto basic = as_basic()) {
                return basic->do_draw(tc);
            }
            return detail::draw_tuple_impl<ResultTuple>(
                gens_, std::index_sequence_for<Ts...>{}, tc);
        }

      private:
        std::tuple<Generator<Ts>...> gens_;
    };
    /// @endcond

    /**
     * @brief Generate tuples from multiple generators.
     *
     * Each generator produces one element of the resulting tuple.
     *
     * @code{.cpp}
     * auto pair = tuples(integers<int>(), text());
     * auto triple = tuples(booleans(), integers<int>(), floats<double>());
     * @endcode
     *
     * @tparam Ts Element types
     * @param gens Generators for each tuple element
     * @return Generator producing tuples
     */
    template <typename... Ts>
    Generator<std::tuple<Ts...>> tuples(Generator<Ts>... gens) {
        return Generator<std::tuple<Ts...>>(
            new TuplesGenerator<Ts...>(std::move(gens)...));
    }

    /// @}

} // namespace hegel::generators
