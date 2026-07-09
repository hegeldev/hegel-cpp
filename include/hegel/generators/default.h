#pragma once

#include "hegel/config.h"

// default_generator (type-directed derivation) is the one feature that needs
// reflect-cpp. Building with HEGEL_REFLECTION=OFF drops it so the rest of the
// library can be consumed from C++17.
#if HEGEL_HAS_REFLECTION

#include <functional>
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

    /// @name Typing
    /// @{

    template <typename T> class DerivedGenerator;

    // Forward declaration — defined below after all specializations
    template <typename T> DerivedGenerator<T> default_generator();

    /// @cond INTERNAL
    namespace detail {

        // =================================================================
        // Type traits for container detection
        // =================================================================

        template <typename T> struct is_vector : std::false_type {};
        template <typename T>
        struct is_vector<std::vector<T>> : std::true_type {};

        template <typename T> struct is_set : std::false_type {};
        template <typename T> struct is_set<std::set<T>> : std::true_type {};

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

        template <typename T, typename Enable = void> struct DefaultGenerator {
            static_assert(
                is_reflectable_struct<T>::value,
                "default_generator<T>(): T must be a supported primitive, "
                "container, or reflectable struct");

            static Generator<T> generator() {
                return compose([](const TestCase& tc) -> T {
                    T result{};
                    auto view = rfl::to_view(result);
                    view.apply([&tc](const auto& field) {
                        using PtrType =
                            typename std::remove_cvref_t<decltype(field)>::Type;
                        using FieldType = std::remove_pointer_t<PtrType>;
                        *field.value() =
                            default_generator<FieldType>().do_draw(tc);
                    });
                    return result;
                });
            }
        };

        // =================================================================
        // Specializations for primitive types
        // =================================================================

        template <> struct DefaultGenerator<bool> {
            static Generator<bool> generator() { return booleans(); }
        };

        template <> struct DefaultGenerator<std::string> {
            static Generator<std::string> generator() { return text(); }
        };

        template <> struct DefaultGenerator<std::monostate> {
            static Generator<std::monostate> generator() {
                return just(std::monostate{});
            }
        };

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<std::is_integral_v<T> &&
                                                    !std::is_same_v<T, bool>>> {
            static Generator<T> generator() { return integers<T>(); }
        };

        template <typename T>
        struct DefaultGenerator<T,
                                std::enable_if_t<std::is_floating_point_v<T>>> {
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
                return maps(default_generator<K>(), default_generator<V>());
            }
        };

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_optional<T>::value>> {
            static Generator<T> generator() {
                using Inner = typename T::value_type;
                return optional(default_generator<Inner>());
            }
        };

        // --- std::tuple<Ts...> ---

        template <typename Tuple, size_t... Is>
        auto make_default_tuple_gen(std::index_sequence<Is...>) {
            return tuples(
                default_generator<std::tuple_element_t<Is, Tuple>>()...);
        }

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_tuple<T>::value>> {
            static Generator<T> generator() {
                return make_default_tuple_gen<T>(
                    std::make_index_sequence<std::tuple_size_v<T>>{});
            }
        };

        // --- std::variant<Ts...> ---

        template <typename Variant, size_t... Is>
        auto make_default_variant_gen(std::index_sequence<Is...>) {
            return variant(default_generator<
                           std::variant_alternative_t<Is, Variant>>()...);
        }

        template <typename T>
        struct DefaultGenerator<T, std::enable_if_t<is_variant<T>::value>> {
            static Generator<T> generator() {
                return make_default_variant_gen<T>(
                    std::make_index_sequence<std::variant_size_v<T>>{});
            }
        };

        // =================================================================
        // Per-field overrides for DerivedGenerator::override()
        // =================================================================

        // Type-erased per-field override. apply() returns true — after
        // drawing the replacement value directly into the object — iff this
        // override targets the field at field_addr within obj.
        template <typename T> struct FieldOverride {
            std::function<bool(T& obj, const void* field_addr,
                               const TestCase& tc)>
                apply;
        };

        template <typename T, typename FieldSpec>
        FieldOverride<T> make_field_override(FieldSpec spec) {
            return FieldOverride<T>{[spec = std::move(spec)](
                                        T& obj, const void* field_addr,
                                        const TestCase& tc) {
                if (static_cast<const void*>(&(obj.*FieldSpec::member_ptr)) !=
                    field_addr) {
                    return false;
                }
                obj.*FieldSpec::member_ptr = spec.generator.do_draw(tc);
                return true;
            }};
        }

        // Struct generator that draws every field in declaration order,
        // consulting the overrides first: an overridden field draws only
        // from its override generator — the default draw is skipped
        // entirely, so it consumes no entropy and adds nothing to the
        // choice sequence.
        template <typename T>
        Generator<T>
        derived_struct_generator(std::vector<FieldOverride<T>> overrides) {
            return compose([overrides =
                                std::move(overrides)](const TestCase& tc) -> T {
                T result{};
                auto view = rfl::to_view(result);
                view.apply([&](const auto& field) {
                    using PtrType =
                        typename std::remove_cvref_t<decltype(field)>::Type;
                    using FieldType = std::remove_pointer_t<PtrType>;
                    // Later overrides shadow earlier ones for the same
                    // field, so search newest-first.
                    for (auto it = overrides.rbegin(); it != overrides.rend();
                         ++it) {
                        if (it->apply(result, field.value(), tc)) {
                            return;
                        }
                    }
                    *field.value() = default_generator<FieldType>().do_draw(tc);
                });
                return result;
            });
        }

    } // namespace detail
    /// @endcond

    // =============================================================================
    // Public API
    // =============================================================================

    /**
     * @brief A Generator produced by default_generator<T>().
     *
     * Behaves exactly like a Generator<T> (it publicly derives from one), but
     * additionally exposes an override() method for supplying per-field
     * generator overrides on struct types.
     *
     * @code{.cpp}
     * auto gen = default_generator<Rectangle>()
     *     .override(field<&Rectangle::width>(integers<int>({.min_value = 1})));
     * @endcode
     *
     * @tparam T The type produced by the generator
     */
    template <typename T> class DerivedGenerator : public Generator<T> {
      public:
        /// @cond INTERNAL
        DerivedGenerator(Generator<T> base) : Generator<T>(std::move(base)) {}
        /// @endcond

        /**
         * @brief Override default per-field generators.
         *
         * Each field specification (from `field<&T::member>(gen)`) replaces
         * the default generator for that member: the overridden member is
         * drawn only from the supplied generator, and the default draw is
         * skipped entirely (it consumes no entropy and adds nothing to the
         * choice sequence, so shrinking is unaffected). Other fields keep
         * the defaults from default_generator<T>().
         *
         * Chained override() calls accumulate. If the same field is
         * overridden more than once, the most recent override wins and is
         * the only one drawn from.
         *
         * @code{.cpp}
         * auto gen = default_generator<Person>()
         *     .override(field<&Person::age>(
         *         integers<int>({.min_value = 0, .max_value = 120})));
         * @endcode
         *
         * @tparam Fields Field specification types (from field<>())
         * @param fields Field specifications for overridden members
         * @return A DerivedGenerator<T> with the overrides applied
         */
        template <typename... Fields>
        DerivedGenerator<T> override(Fields... fields) const {
            std::vector<detail::FieldOverride<T>> overrides = overrides_;
            (overrides.push_back(
                 detail::make_field_override<T>(std::move(fields))),
             ...);
            DerivedGenerator<T> result(
                detail::derived_struct_generator<T>(overrides));
            result.overrides_ = std::move(overrides);
            return result;
        }

      private:
        // The overrides this generator was built with, carried so chained
        // override() calls extend the set instead of stacking draws on top
        // of a fully drawn base.
        std::vector<detail::FieldOverride<T>> overrides_;
    };

    /**
     * @brief Create a default generator for type T.
     *
     * Dispatches to the appropriate built-in generator based on the type:
     * - Primitive types: bool, integers, floats, std::string
     * - Containers: vector, set, map, optional, tuple, variant
     * - Reflected structs: any struct with public fields (via reflect-cpp)
     *
     * For structs, each field is generated using default_generator for its
     * type. Call `.override(...)` on the returned generator to customize
     * individual fields:
     *
     * @code{.cpp}
     * struct Person { std::string name; int age; };
     *
     * auto gen = default_generator<Person>()
     *     .override(field<&Person::age>(
     *         integers<int>({.min_value = 0, .max_value = 120})));
     * @endcode
     *
     * @tparam T The type to generate
     * @return A DerivedGenerator<T> (usable anywhere a Generator<T> is)
     */
    template <typename T> DerivedGenerator<T> default_generator() {
        return DerivedGenerator<T>(detail::DefaultGenerator<T>::generator());
    }

    /// @}

} // namespace hegel::generators

#endif // HEGEL_HAS_REFLECTION
