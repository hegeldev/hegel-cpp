#pragma once

/**
 * @file derived.h
 * @brief Default and derived generators: automatic generator dispatch
 *        for primitive types, containers, and reflected structs.
 *
 * This header must be included after all other strategy headers because
 * default_generator<T>() dispatches to integers<T>(), text(), etc.
 */

#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include <rfl.hpp>

#include "hegel/core.h"
#include "hegel/generators/builds.h"
#include "hegel/generators/collections.h"
#include "hegel/generators/combinators.h"
#include "hegel/generators/numeric.h"
#include "hegel/generators/primitives.h"
#include "hegel/generators/strings.h"

namespace hegel::generators {

    // Forward declaration — defined below after all specializations
    template <typename T> Generator<T> default_generator();

    /// @cond INTERNAL
    namespace detail {

        // =================================================================
        // Type traits for container detection
        // =================================================================

        template <typename T> struct is_vector : std::false_type {};
        template <typename T>
        struct is_vector<std::vector<T>> : std::true_type {};

        template <typename T> struct is_set : std::false_type {};
        template <typename T>
        struct is_set<std::set<T>> : std::true_type {};

        template <typename T> struct is_map : std::false_type {};
        template <typename K, typename V>
        struct is_map<std::map<K, V>> : std::true_type {};

        template <typename T> struct is_optional : std::false_type {};
        template <typename T>
        struct is_optional<std::optional<T>> : std::true_type {};

        template <typename T> struct is_tuple : std::false_type {};
        template <typename... Ts>
        struct is_tuple<std::tuple<Ts...>> : std::true_type {};

        template <typename T> struct is_variant : std::false_type {};
        template <typename... Ts>
        struct is_variant<std::variant<Ts...>> : std::true_type {};

        // =================================================================
        // Reflectable struct detection
        // =================================================================

        template <typename T, typename = void>
        struct is_reflectable_struct : std::false_type {};

        template <typename T>
        struct is_reflectable_struct<
            T, std::void_t<decltype(rfl::to_view(std::declval<T&>()))>>
            : std::true_type {};

        // =================================================================
        // DefaultGenerator trait — primary template (struct fallback)
        // =================================================================

        template <typename T, typename Enable = void>
        struct DefaultGenerator {
            static_assert(
                is_reflectable_struct<T>::value,
                "default_generator<T>(): T must be a supported primitive, "
                "container, or reflectable struct");

            static Generator<T> generator() {
                return from_function<T>([](TestCaseData* data) -> T {
                    T result{};
                    auto view = rfl::to_view(result);
                    view.apply([data](const auto& field) {
                        using PtrType = typename std::remove_cvref_t<
                            decltype(field)>::Type;
                        using FieldType = std::remove_pointer_t<PtrType>;
                        *field.value() = default_generator<FieldType>()
                                             .do_draw(data);
                    });
                    return result;
                });
            }
        };

        // =================================================================
        // Specializations for primitive types
        // =================================================================

        template <>
        struct DefaultGenerator<bool> {
            static Generator<bool> generator() { return booleans(); }
        };

        template <>
        struct DefaultGenerator<std::string> {
            static Generator<std::string> generator() { return text(); }
        };

        template <>
        struct DefaultGenerator<std::monostate> {
            static Generator<std::monostate> generator() { return nulls(); }
        };

        template <typename T>
        struct DefaultGenerator<
            T, std::enable_if_t<std::is_integral_v<T> &&
                                !std::is_same_v<T, bool>>> {
            static Generator<T> generator() { return integers<T>(); }
        };

        template <typename T>
        struct DefaultGenerator<
            T, std::enable_if_t<std::is_floating_point_v<T>>> {
            static Generator<T> generator() { return floats<T>(); }
        };

        // =================================================================
        // Specializations for standard containers
        // =================================================================

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_vector<T>::value>> {
            static Generator<T> generator() {
                using Elem = typename T::value_type;
                return vectors(default_generator<Elem>());
            }
        };

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_set<T>::value>> {
            static Generator<T> generator() {
                using Elem = typename T::value_type;
                return sets(default_generator<Elem>());
            }
        };

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_map<T>::value>> {
            static Generator<T> generator() {
                using K = typename T::key_type;
                using V = typename T::mapped_type;
                return dictionaries(default_generator<K>(),
                                    default_generator<V>());
            }
        };

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_optional<T>::value>> {
            static Generator<T> generator() {
                using Inner = typename T::value_type;
                return optional_(default_generator<Inner>());
            }
        };

        // --- std::tuple<Ts...> ---

        template <typename Tuple, size_t... Is>
        auto make_default_tuple_gen(std::index_sequence<Is...>) {
            return tuples(default_generator<
                          std::tuple_element_t<Is, Tuple>>()...);
        }

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_tuple<T>::value>> {
            static Generator<T> generator() {
                return make_default_tuple_gen<T>(
                    std::make_index_sequence<
                        std::tuple_size_v<T>>{});
            }
        };

        // --- std::variant<Ts...> ---

        template <typename Variant, size_t... Is>
        auto make_default_variant_gen(std::index_sequence<Is...>) {
            return variant_(
                default_generator<
                    std::variant_alternative_t<Is, Variant>>()...);
        }

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_variant<T>::value>> {
            static Generator<T> generator() {
                return make_default_variant_gen<T>(
                    std::make_index_sequence<
                        std::variant_size_v<T>>{});
            }
        };

    } // namespace detail
    /// @endcond

    // =============================================================================
    // Public API
    // =============================================================================

    /**
     * @brief Create a default generator for type T.
     *
     * Dispatches to the appropriate built-in strategy based on the type:
     * - Primitive types: bool, integers, floats, std::string
     * - Containers: vector, set, map, optional, tuple, variant
     * - Reflected structs: any struct with public fields (via reflect-cpp)
     *
     * For structs, each field is generated using default_generator for its
     * type. Nested structs are supported recursively.
     *
     * @code{.cpp}
     * struct Point { double x; double y; };
     *
     * auto gen = hegel::generators::default_generator<Point>();
     * Point p = hegel::draw(gen);
     * @endcode
     *
     * @tparam T The type to generate
     * @return A Generator<T> instance
     */
    template <typename T> Generator<T> default_generator() {
        return detail::DefaultGenerator<T>::generator();
    }

    /**
     * @brief Create a derived generator for a struct with field overrides.
     *
     * Generates all fields using their default generators, then applies
     * any field overrides specified via field<&T::member>(generator).
     *
     * @code{.cpp}
     * struct Person {
     *     std::string name;
     *     int age;
     * };
     *
     * // Override only the age field; name uses default_generator<string>
     * auto gen = hegel::generators::derive<Person>(
     *     field<&Person::age>(integers<int>({.min_value = 0,
     *                                       .max_value = 120}))
     * );
     * @endcode
     *
     * @tparam T The struct type to generate
     * @tparam Overrides Field override types (from field<>())
     * @param overrides Field specifications for overridden fields
     * @return A Generator<T> with defaults plus overrides
     */
    template <typename T, typename... Overrides>
    Generator<T> derive(Overrides... overrides) {
        auto override_tuple = std::make_tuple(std::move(overrides)...);

        return from_function<T>(
            [override_tuple](TestCaseData* data) mutable -> T {
                T result{};
                auto view = rfl::to_view(result);

                // Draw all fields from their default generators
                view.apply([data](const auto& field) {
                    using PtrType =
                        typename std::remove_cvref_t<decltype(field)>::Type;
                    using FieldType = std::remove_pointer_t<PtrType>;
                    *field.value() =
                        default_generator<FieldType>().do_draw(data);
                });

                // Apply overrides (replaces default-generated values)
                std::apply(
                    [&result, data](auto&... fs) {
                        ((result.*(std::remove_reference_t<
                                       decltype(fs)>::member_ptr) =
                              fs.generator.do_draw(data)),
                         ...);
                    },
                    override_tuple);

                return result;
            });
    }

} // namespace hegel::generators
