#pragma once

/**
 * @file collections.h
 * @brief Collection generator functions: vectors, sets, dictionaries, tuples
 */

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
     * @brief Parameters for vectors() strategy.
     */
    struct VectorsParams {
        size_t min_size = 0;            ///< Minimum vector size
        std::optional<size_t> max_size; ///< Maximum vector size. Default: 100
        bool unique = false; ///< If true, all elements must be unique
    };

    /**
     * @brief Parameters for sets() strategy.
     */
    struct SetsParams {
        size_t min_size = 0;            ///< Minimum set size
        std::optional<size_t> max_size; ///< Maximum set size. Default: 20
    };

    /**
     * @brief Parameters for dictionaries() strategy.
     */
    struct DictionariesParams {
        size_t min_size = 0; ///< Minimum number of entries
        std::optional<size_t>
            max_size; ///< Maximum number of entries. Default: 20
    };

    // =============================================================================
    // Template strategies
    // =============================================================================

    /// @name Collection Strategies
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
        if (params.max_size && params.min_size > *params.max_size) {
            throw std::invalid_argument(
                "Cannot have max_size < min_size");
        }

        if (elements.schema()) {
            nlohmann::json schema = {{"type", "list"},
                                     {"elements", *elements.schema()},
                                     {"min_size", params.min_size},
                                     {"unique", params.unique}};

            if (params.max_size)
                schema["max_size"] = *params.max_size;

            return from_schema<std::vector<T>>(std::move(schema));
        }

        size_t max_size = params.max_size.value_or(100);

        auto length_gen = integers<size_t>(
            {.min_value = params.min_size, .max_value = max_size});

        return from_function<std::vector<T>>(
            [elements, length_gen](TestCaseData* data) {
                size_t len = length_gen.do_draw(data);
                std::vector<T> result;
                result.reserve(len);

                for (size_t i = 0; i < len; ++i) {
                    result.push_back(elements.do_draw(data));
                }

                return result;
            });
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
        if (params.max_size && params.min_size > *params.max_size) {
            throw std::invalid_argument(
                "Cannot have max_size < min_size");
        }

        if (elements.schema()) {
            nlohmann::json schema = {{"type", "list"},
                                     {"elements", *elements.schema()},
                                     {"min_size", params.min_size},
                                     {"unique", true}};

            if (params.max_size)
                schema["max_size"] = *params.max_size;

            auto vec_gen = from_schema<std::vector<T>>(std::move(schema));

            return from_function<std::set<T>>([vec_gen](TestCaseData* data) {
                auto vec = vec_gen.do_draw(data);
                return std::set<T>(vec.begin(), vec.end());
            });
        }

        size_t max_size = params.max_size.value_or(20);
        auto length_gen = integers<size_t>(
            {.min_value = params.min_size, .max_value = max_size});

        return from_function<std::set<T>>([elements,
                                           length_gen](TestCaseData* data) {
            size_t target_len = length_gen.do_draw(data);
            std::set<T> result;

            size_t attempts = 0;
            while (result.size() < target_len && attempts < target_len * 10) {
                result.insert(elements.do_draw(data));
                ++attempts;
            }

            return result;
        });
    }

    /**
     * @brief Generate dictionaries (maps) with configurable key and value
     * types.
     *
     * @code{.cpp}
     * // String keys
     * auto strDict = dictionaries(text(), integers<int>());
     *
     * // Integer keys
     * auto intDict = dictionaries(integers<int>(), text());
     *
     * // With size bounds
     * auto bounded = dictionaries(text(), integers<int>(), {.min_size = 1,
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
    Generator<std::map<K, V>> dictionaries(Generator<K> keys,
                                           Generator<V> values,
                                           DictionariesParams params = {}) {
        if (params.max_size && params.min_size > *params.max_size) {
            throw std::invalid_argument(
                "Cannot have max_size < min_size");
        }

        if (keys.schema() && values.schema()) {
            nlohmann::json schema = {{"type", "dict"},
                                     {"keys", *keys.schema()},
                                     {"values", *values.schema()},
                                     {"min_size", params.min_size}};

            if (params.max_size)
                schema["max_size"] = *params.max_size;

            // Wire format is [[key, value], ...], deserialize as vector of
            // pairs
            auto vec_gen =
                from_schema<std::vector<std::pair<K, V>>>(std::move(schema));

            return from_function<std::map<K, V>>([vec_gen](TestCaseData* data) {
                auto pairs = vec_gen.do_draw(data);
                return std::map<K, V>(pairs.begin(), pairs.end());
            });
        }

        size_t max_size = params.max_size.value_or(20);
        auto length_gen = integers<size_t>(
            {.min_value = params.min_size, .max_value = max_size});

        return from_function<std::map<K, V>>(
            [keys, values, length_gen](TestCaseData* data) {
                size_t len = length_gen.do_draw(data);
                std::map<K, V> result;

                while (result.size() < len) {
                    K key = keys.do_draw(data);
                    V value = values.do_draw(data);
                    result[std::move(key)] = std::move(value);
                }

                return result;
            });
    }

    /// @cond INTERNAL
    namespace detail {

        template <typename... Gens>
        auto make_tuple_schema(const Gens&... gens)
            -> std::optional<nlohmann::json> {
            bool all_have_schemas = (gens.schema().has_value() && ...);
            if (!all_have_schemas)
                return std::nullopt;

            nlohmann::json elements = nlohmann::json::array();
            (elements.push_back(*gens.schema()), ...);

            nlohmann::json schema = {{"type", "tuple"}, {"elements", elements}};

            return schema;
        }

        template <typename Tuple, typename GenTuple, size_t... Is>
        Tuple draw_tuple_impl(const GenTuple& gens, std::index_sequence<Is...>,
                              TestCaseData* data) {
            return Tuple{std::get<Is>(gens).do_draw(data)...};
        }

    } // namespace detail
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
        using ResultTuple = std::tuple<Ts...>;

        auto maybe_schema = detail::make_tuple_schema(gens...);

        if (maybe_schema) {
            return from_schema<ResultTuple>(std::move(*maybe_schema));
        }

        auto gen_tuple = std::make_tuple(std::move(gens)...);

        return from_function<ResultTuple>([gen_tuple](TestCaseData* data) {
            return detail::draw_tuple_impl<ResultTuple>(
                gen_tuple, std::index_sequence_for<Ts...>{}, data);
        });
    }

    /// @}

} // namespace hegel::generators
