#pragma once

/**
 * @file numeric.h
 * @brief Numeric generator functions: integers, floats
 */

#include <limits>

#include "hegel/core.h"

namespace hegel::generators {

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

    // =============================================================================
    // Template strategies
    // =============================================================================

    /// @name Numeric Strategies
    /// @{

    /**
     * @brief Generate random integers. For a given integral type T, produces
     * values in the range [std::numeric_limits<T>::min(),
     * std::numeric_limits<T>::max()] by default.
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

        if (min_val > max_val) {
            throw std::invalid_argument("Cannot have max_value < min_value");
        }

        hegel::internal::json::json schema = {{"type", "integer"},
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

        if (nan && (has_min || has_max)) {
            throw std::invalid_argument(
                "Cannot have allow_nan=true with min_value or max_value");
        }
        if (has_min && has_max && *params.min_value > *params.max_value) {
            throw std::invalid_argument("Cannot have max_value < min_value");
        }
        if (inf && has_min && has_max) {
            throw std::invalid_argument(
                "Cannot have allow_infinity=true with both min_value and "
                "max_value");
        }

        hegel::internal::json::json schema = {
            {"type", "float"},
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

} // namespace hegel::generators
