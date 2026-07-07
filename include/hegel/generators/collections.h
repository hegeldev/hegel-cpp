#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "hegel/core.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for vectors() generator.
     */
    struct VectorsParams {
        size_t min_size = 0;            ///< Minimum vector size
        std::optional<size_t> max_size; ///< Maximum vector size. Default: no
                                        ///< limit (engine-chosen sizes)
        bool unique = false; ///< If true, all elements must be unique
    };

    /**
     * @brief Parameters for sets() generator.
     */
    struct SetsParams {
        size_t min_size = 0;            ///< Minimum set size
        std::optional<size_t> max_size; ///< Maximum set size. Default: no
                                        ///< limit (engine-chosen sizes)
    };

    /**
     * @brief Parameters for maps() generator.
     */
    struct MapsParams {
        size_t min_size = 0;            ///< Minimum number of entries
        std::optional<size_t> max_size; ///< Maximum number of entries.
                                        ///< Default: no limit (engine-chosen
                                        ///< sizes)
    };

    /// @cond INTERNAL
    namespace detail {

        // True iff `a == b` is well-formed for const T. vectors() only
        // needs operator== when unique=true, so its availability is checked
        // at runtime rather than required of every element type.
        template <typename T, typename = void>
        struct is_equality_comparable : std::false_type {};
        template <typename T>
        struct is_equality_comparable<
            T, std::void_t<decltype(std::declval<const T&>() ==
                                    std::declval<const T&>())>>
            : std::true_type {};

        inline uint64_t
        collection_max_size(const std::optional<size_t>& max_size) {
            return max_size ? static_cast<uint64_t>(*max_size)
                            : hegel::internal::no_max_size;
        }

    } // namespace detail

    // Concrete IGenerator for vectors(). An engine-managed collection
    // decides how many elements to draw; uniqueness is enforced by
    // rejecting duplicate elements back to the engine.
    template <typename T>
    class VectorsGenerator : public IGenerator<std::vector<T>> {
      public:
        VectorsGenerator(Generator<T> elements, VectorsParams params = {})
            : elements_(std::move(elements)), params_(params) {
            if (params_.max_size && params_.min_size > *params_.max_size) {
                throw std::invalid_argument("Cannot have max_size < min_size");
            }
            if (params_.unique && !detail::is_equality_comparable<T>::value) {
                throw std::invalid_argument(
                    "vectors(..., {.unique = true}) requires an "
                    "equality-comparable element type");
            }
        }

        std::vector<T> do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            hi::start_span(tc, hi::SpanLabel::List);
            int64_t collection = hi::new_collection(
                tc, params_.min_size,
                detail::collection_max_size(params_.max_size));
            std::vector<T> result;
            while (hi::collection_more(tc, collection)) {
                T element = elements_.do_draw(tc);
                if constexpr (detail::is_equality_comparable<T>::value) {
                    if (params_.unique &&
                        std::find(result.begin(), result.end(), element) !=
                            result.end()) {
                        hi::collection_reject(tc, collection,
                                              "duplicate element");
                        continue;
                    }
                }
                result.push_back(std::move(element));
            }
            hi::stop_span(tc);
            return result;
        }

      private:
        Generator<T> elements_;
        VectorsParams params_;
    };

    // Concrete IGenerator for sets(). Duplicate elements are rejected back
    // to the engine so it draws a replacement.
    template <typename T> class SetsGenerator : public IGenerator<std::set<T>> {
      public:
        SetsGenerator(Generator<T> elements, SetsParams params = {})
            : elements_(std::move(elements)), params_(params) {
            if (params_.max_size && params_.min_size > *params_.max_size) {
                throw std::invalid_argument("Cannot have max_size < min_size");
            }
        }

        std::set<T> do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            hi::start_span(tc, hi::SpanLabel::Set);
            int64_t collection = hi::new_collection(
                tc, params_.min_size,
                detail::collection_max_size(params_.max_size));
            std::set<T> result;
            while (hi::collection_more(tc, collection)) {
                if (!result.insert(elements_.do_draw(tc)).second) {
                    hi::collection_reject(tc, collection, "duplicate element");
                }
            }
            hi::stop_span(tc);
            return result;
        }

      private:
        Generator<T> elements_;
        SetsParams params_;
    };

    // Concrete IGenerator for maps(). Draws the key first and only draws a
    // value once the key is known to be fresh; duplicate keys are rejected
    // back to the engine.
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

        std::map<K, V> do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            hi::start_span(tc, hi::SpanLabel::Map);
            int64_t collection = hi::new_collection(
                tc, params_.min_size,
                detail::collection_max_size(params_.max_size));
            std::map<K, V> result;
            while (hi::collection_more(tc, collection)) {
                K key = keys_.do_draw(tc);
                if (result.find(key) != result.end()) {
                    hi::collection_reject(tc, collection, "duplicate key");
                    continue;
                }
                result.emplace(std::move(key), values_.do_draw(tc));
            }
            hi::stop_span(tc);
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

    // Concrete IGenerator for tuples(). Draws each element in order inside
    // one tuple span.
    template <typename... Ts>
    class TuplesGenerator : public IGenerator<std::tuple<Ts...>> {
      public:
        using ResultTuple = std::tuple<Ts...>;

        explicit TuplesGenerator(Generator<Ts>... gens)
            : gens_(std::move(gens)...) {}

        ResultTuple do_draw(const TestCase& tc) const override {
            namespace hi = hegel::internal;
            hi::start_span(tc, hi::SpanLabel::Tuple);
            ResultTuple result = detail::draw_tuple_impl<ResultTuple>(
                gens_, std::index_sequence_for<Ts...>{}, tc);
            hi::stop_span(tc);
            return result;
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
