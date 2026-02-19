#pragma once

/**
 * @file strategies.h
 * @brief Strategy functions for data generation
 *
 * Contains all strategy functions in the hegel::strategies namespace for
 * generating primitive types, collections, and composite data structures.
 */

#include <optional>
#include <string>
#include <variant>

#include "generators.h"

/**
 * @brief Namespace containing all strategy functions for data generation.
 *
 * Strategies are factory functions that create Generator instances with
 * specific generation behaviors. Import this namespace to use the
 * strategy functions directly.
 *
 * @code{.cpp}
 * using namespace hegel::strategies;
 *
 * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
 * auto str_gen = text({.min_size = 1, .max_size = 50});
 * auto bool_gen = booleans();
 * @endcode
 */
namespace hegel::strategies {
    using hegel::generators::from_function;
    using hegel::generators::from_schema;
    using hegel::generators::Generator;

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for integers() strategy.
     * @tparam T The integer type
     */
    template <typename T> struct IntegersParams {
        std::optional<T>
            min_value; ///< Minimum value (inclusive). Default: type minimum
        std::optional<T>
            max_value; ///< Maximum value (inclusive). Default: type maximum
    };

    /**
     * @brief Parameters for floats() strategy.
     * @tparam T The floating point type
     */
    template <typename T> struct FloatsParams {
        std::optional<T> min_value; ///< Minimum value. Default: no minimum
        std::optional<T> max_value; ///< Maximum value. Default: no maximum
        bool exclude_min =
            false; ///< If true, exclude min_value (exclusive bound)
        bool exclude_max =
            false; ///< If true, exclude max_value (exclusive bound)
        std::optional<bool>
            allow_nan; ///< Allow NaN. Default: true if unbounded
        std::optional<bool>
            allow_infinity; ///< Allow infinity. Default: true if unbounded
    };

    /**
     * @brief Parameters for text() strategy.
     */
    struct TextParams {
        size_t min_size = 0; ///< Minimum string length
        std::optional<size_t>
            max_size; ///< Maximum string length. Default: no limit
    };

    /**
     * @brief Parameters for binary() strategy.
     */
    struct BinaryParams {
        size_t min_size = 0; ///< Minimum size in bytes
        std::optional<size_t>
            max_size; ///< Maximum size in bytes. Default: no limit
    };

    /**
     * @brief Parameters for domains() strategy.
     */
    struct DomainsParams {
        size_t max_length = 255; ///< Maximum domain name length
    };

    /**
     * @brief Parameters for ip_addresses() strategy.
     */
    struct IpAddressesParams {
        std::optional<int> v; ///< IP version: 4, 6, or nullopt for both
    };

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
    // Non-template strategy declarations (implemented in strategies.cpp)
    // =============================================================================

    /// @name Primitive Strategies
    /// @{

    /// Generate null values (std::monostate)
    Generator<std::monostate> nulls();

    /// Generate random boolean values
    Generator<bool> booleans();

    /// @}

    /// @name String Strategies
    /// @{

    /**
     * @brief Generate random text strings.
     * @param params Length constraints
     * @return Generator producing random strings
     */
    Generator<std::string> text(TextParams params = {});

    /**
     * @brief Generate random binary data (byte sequences).
     * @param params Size constraints
     * @return Generator producing byte vectors
     */
    Generator<std::vector<uint8_t>> binary(BinaryParams params = {});

    /**
     * @brief Generate strings matching a regular expression.
     * @param pattern Regex pattern to match
     * @param fullmatch If true, the entire string must match the pattern; if
     * false (default), the string just needs to contain a match
     * @return Generator producing matching strings
     */
    Generator<std::string> from_regex(const std::string& pattern,
                                      bool fullmatch = false);

    /// Generate valid email addresses
    Generator<std::string> emails();

    /**
     * @brief Generate valid domain names.
     * @param params Length constraints
     * @return Generator producing domain names
     */
    Generator<std::string> domains(DomainsParams params = {});

    /// Generate valid URLs
    Generator<std::string> urls();

    /**
     * @brief Generate IP addresses.
     * @param params Version constraints (v=4, v=6, or both)
     * @return Generator producing IP address strings
     */
    Generator<std::string> ip_addresses(IpAddressesParams params = {});

    /// Generate dates in ISO 8601 format (YYYY-MM-DD)
    Generator<std::string> dates();

    /// Generate times in ISO 8601 format (HH:MM:SS)
    Generator<std::string> times();

    /// Generate datetimes in ISO 8601 format
    Generator<std::string> datetimes();

    /// @}

    // =============================================================================
    // Template strategies (must be in header)
    // =============================================================================

    /// @name Constant and Numeric Strategies
    /// @{

    /**
     * @brief Generate a constant value.
     *
     * Always returns the same value. Useful for creating fixed elements
     * in composite generators.
     *
     * @code{.cpp}
     * auto answer = just(42);
     * auto greeting = just("hello");
     * @endcode
     *
     * @tparam T The value type
     * @param value The constant value to generate
     * @return Generator that always produces value
     */
    template <typename T> Generator<T> just(T value) {
        if constexpr (std::is_same_v<T, bool> ||
                      std::is_same_v<T, std::nullptr_t> ||
                      std::is_integral_v<T> || std::is_floating_point_v<T> ||
                      std::is_same_v<T, std::string>) {
            nlohmann::json schema = {{"const", value}};
            return from_function<T>([value]() { return value; },
                                    std::move(schema));
        } else {
            return from_function<T>([value]() { return value; });
        }
    }

    /// @overload just(const char*) - convenience overload for string literals
    inline Generator<std::string> just(const char* value) {
        return just(std::string(value));
    }

    /**
     * @brief Generate random integers.
     *
     * @code{.cpp}
     * auto any_int = integers<int>();
     * auto bounded = integers<int>({.min_value = 0, .max_value = 100});
     * auto positive = integers<int>({.min_value = 1});
     * @endcode
     *
     * @tparam T Integer type (default: int64_t)
     * @param params Bounds constraints
     * @return Generator producing integers in the specified range
     */
    template <typename T = int64_t>
        requires std::is_integral_v<T>
    Generator<T> integers(IntegersParams<T> params = {}) {
        T min_val = params.min_value.value_or(std::numeric_limits<T>::min());
        T max_val = params.max_value.value_or(std::numeric_limits<T>::max());

        nlohmann::json schema = {{"type", "integer"},
                                 {"min_value", min_val},
                                 {"max_value", max_val}};

        return from_schema<T>(std::move(schema));
    }

    /**
     * @brief Generate random floating point numbers.
     *
     * @code{.cpp}
     * auto any_float = floats<double>();
     * auto unit = floats<double>({.min_value = 0.0, .max_value = 1.0});
     * auto open = floats<double>({
     *     .min_value = 0.0, .max_value = 1.0,
     *     .exclude_min = true, .exclude_max = true
     * });
     * @endcode
     *
     * @tparam T Floating point type (default: double)
     * @param params Bounds and exclusion constraints
     * @return Generator producing floats in the specified range
     */
    template <typename T = double>
        requires std::is_floating_point_v<T>
    Generator<T> floats(FloatsParams<T> params = {}) {
        // Determine width from type size (float = 32 bits, double = 64 bits)
        constexpr int width = sizeof(T) * 8;

        bool has_min = params.min_value.has_value();
        bool has_max = params.max_value.has_value();
        bool nan = params.allow_nan.value_or(!has_min && !has_max);
        bool inf = params.allow_infinity.value_or(!has_min || !has_max);

        nlohmann::json schema = {{"type", "number"},
                                 {"exclude_min", params.exclude_min},
                                 {"exclude_max", params.exclude_max},
                                 {"allow_nan", nan},
                                 {"allow_infinity", inf},
                                 {"width", width}};

        if (params.min_value) {
            schema["min_value"] = *params.min_value;
        }

        if (params.max_value) {
            schema["max_value"] = *params.max_value;
        }

        return from_schema<T>(std::move(schema));
    }

    /// @}

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
        if (elements.schema()) {
            // Use "set" type for unique, "list" type otherwise
            std::string schema_type = params.unique ? "set" : "list";
            nlohmann::json schema = {{"type", schema_type},
                                     {"elements", *elements.schema()},
                                     {"min_size", params.min_size}};

            if (params.max_size)
                schema["max_size"] = *params.max_size;

            return from_schema<std::vector<T>>(std::move(schema));
        }

        size_t max_size = params.max_size.value_or(100);

        auto length_gen = integers<size_t>(
            {.min_value = params.min_size, .max_value = max_size});

        return from_function<std::vector<T>>([elements, length_gen]() {
            size_t len = length_gen.generate();
            std::vector<T> result;
            result.reserve(len);

            for (size_t i = 0; i < len; ++i) {
                result.push_back(elements.generate());
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
        if (elements.schema()) {
            nlohmann::json schema = {{"type", "set"},
                                     {"elements", *elements.schema()},
                                     {"min_size", params.min_size}};

            if (params.max_size)
                schema["max_size"] = *params.max_size;

            auto vec_gen = from_schema<std::vector<T>>(std::move(schema));

            return from_function<std::set<T>>([vec_gen]() {
                auto vec = vec_gen.generate();
                return std::set<T>(vec.begin(), vec.end());
            });
        }

        size_t max_size = params.max_size.value_or(20);
        auto length_gen = integers<size_t>(
            {.min_value = params.min_size, .max_value = max_size});

        return from_function<std::set<T>>([elements, length_gen]() {
            size_t target_len = length_gen.generate();
            std::set<T> result;

            size_t attempts = 0;
            while (result.size() < target_len && attempts < target_len * 10) {
                result.insert(elements.generate());
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

            return from_function<std::map<K, V>>([vec_gen]() {
                auto pairs = vec_gen.generate();
                return std::map<K, V>(pairs.begin(), pairs.end());
            });
        }

        size_t max_size = params.max_size.value_or(20);
        auto length_gen = integers<size_t>(
            {.min_value = params.min_size, .max_value = max_size});

        return from_function<std::map<K, V>>([keys, values, length_gen]() {
            size_t len = length_gen.generate();
            std::map<K, V> result;

            while (result.size() < len) {
                K key = keys.generate();
                V value = values.generate();
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
        Tuple generate_tuple_impl(const GenTuple& gens,
                                  std::index_sequence<Is...>) {
            return Tuple{std::get<Is>(gens).generate()...};
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

        return from_function<ResultTuple>([gen_tuple]() {
            return detail::generate_tuple_impl<ResultTuple>(
                gen_tuple, std::index_sequence_for<Ts...>{});
        });
    }

    /// @}

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
    template <typename T> Generator<T> sampled_from(std::vector<T> elements) {
        internal::assume(!elements.empty());

        if constexpr (std::is_same_v<T, bool> ||
                      std::is_same_v<T, std::nullptr_t> ||
                      std::is_integral_v<T> || std::is_floating_point_v<T> ||
                      std::is_same_v<T, std::string>) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : elements)
                arr.push_back(e);
            nlohmann::json schema = {{"sampled_from", arr}};
            return from_schema<T>(std::move(schema));
        } else {
            auto index_gen = integers<size_t>(
                {.min_value = 0, .max_value = elements.size() - 1});

            return from_function<T>([elements, index_gen]() {
                size_t idx = index_gen.generate();
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
        return sampled_from(std::move(strings));
    }

    /// @cond INTERNAL
    namespace detail {

        template <typename T>
        auto make_one_of_schema(const std::vector<Generator<T>>& gens)
            -> std::optional<nlohmann::json> {
            for (const auto& gen : gens) {
                if (!gen.schema())
                    return std::nullopt;
            }

            nlohmann::json one_of_arr = nlohmann::json::array();
            for (const auto& gen : gens) {
                one_of_arr.push_back(*gen.schema());
            }

            nlohmann::json schema = {{"one_of", one_of_arr}};
            return schema;
        }

    } // namespace detail
    /// @endcond

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
        assume(!gens.empty());

        auto maybe_schema = detail::make_one_of_schema(gens);

        if (maybe_schema) {
            return from_schema<T>(std::move(*maybe_schema));
        }

        auto index_gen =
            integers<size_t>({.min_value = 0, .max_value = gens.size() - 1});

        return from_function<T>([gens, index_gen]() {
            size_t idx = index_gen.generate();
            return gens[idx].generate();
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
        Variant generate_variant_impl(const GenTuple& gens, size_t idx) {
            if constexpr (I < std::tuple_size_v<GenTuple>) {
                if (idx == I) {
                    return std::get<I>(gens).generate();
                }
                return generate_variant_impl<Variant, GenTuple, I + 1>(gens,
                                                                       idx);
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

        return from_function<ResultVariant>([gen_tuple, index_gen]() {
            size_t idx = index_gen.generate();
            return detail::generate_variant_impl<ResultVariant,
                                                 decltype(gen_tuple)>(gen_tuple,
                                                                      idx);
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
            [gen, bool_gen]() -> std::optional<T> {
                bool is_none = bool_gen.generate();
                if (is_none) {
                    return std::nullopt;
                }
                return gen.generate();
            });
    }

    /// @}

    /// @name Object Construction Strategies
    /// @{

    /**
     * @brief Construct objects using positional constructor arguments.
     *
     * Generates constructor arguments from the provided generators and
     * calls T's constructor.
     *
     * @code{.cpp}
     * struct Point {
     *     Point(double x, double y) : x(x), y(y) {}
     *     double x, y;
     * };
     *
     * auto point = builds<Point>(
     *     floats<double>({.min_value = 0, .max_value = 100}),
     *     floats<double>({.min_value = 0, .max_value = 100})
     * );
     * @endcode
     *
     * @tparam T Type to construct
     * @tparam Gens Generator types for constructor arguments
     * @param gens Generators providing constructor arguments
     * @return Generator producing T instances
     */
    template <typename T, typename... Gens> Generator<T> builds(Gens... gens) {
        auto gen_tuple = std::make_tuple(std::move(gens)...);

        return from_function<T>([gen_tuple]() {
            return std::apply([](auto&... g) { return T(g.generate()...); },
                              gen_tuple);
        });
    }

    /**
     * @brief Helper for named field initialization in builds_agg().
     *
     * @tparam MemberPtr Pointer-to-member for the field
     * @tparam Gen Generator type for the field value
     */
    template <auto MemberPtr, typename Gen> struct Field {
        static constexpr auto member_ptr = MemberPtr; ///< The member pointer
        Gen generator; ///< Generator for the field value
    };

    /**
     * @brief Create a field specification for builds_agg().
     *
     * @code{.cpp}
     * auto rect = builds_agg<Rectangle>(
     *     field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value =
     * 100})), field<&Rectangle::height>(integers<int>({.min_value = 1,
     * .max_value = 100}))
     * );
     * @endcode
     *
     * @tparam MemberPtr Pointer-to-member specifying which field
     * @tparam Gen Generator type
     * @param gen Generator for the field value
     * @return A Field specification for use with builds_agg()
     */
    template <auto MemberPtr, typename Gen>
    Field<MemberPtr, Gen> field(Gen gen) {
        return Field<MemberPtr, Gen>{std::move(gen)};
    }

    /**
     * @brief Construct aggregates using named field initialization.
     *
     * Useful for structs where you want to specify fields by name rather
     * than position. Each field() specifies a member pointer and generator.
     *
     * @code{.cpp}
     * struct Rectangle {
     *     int width;
     *     int height;
     * };
     *
     * auto rect = builds_agg<Rectangle>(
     *     field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value =
     * 100})), field<&Rectangle::height>(integers<int>({.min_value = 1,
     * .max_value = 100}))
     * );
     * @endcode
     *
     * @tparam T Aggregate type to construct
     * @tparam Fields Field specification types
     * @param fields Field specifications from field()
     * @return Generator producing T instances
     */
    template <typename T, typename... Fields>
    Generator<T> builds_agg(Fields... fields) {
        auto gen_tuple = std::make_tuple(std::move(fields)...);

        return from_function<T>([gen_tuple]() mutable {
            T result{};
            std::apply(
                [&result](auto&... fs) {
                    ((result.*
                          (std::remove_reference_t<decltype(fs)>::member_ptr) =
                          fs.generator.generate()),
                     ...);
                },
                gen_tuple);
            return result;
        });
    }

    /// @}

} // namespace hegel::strategies
