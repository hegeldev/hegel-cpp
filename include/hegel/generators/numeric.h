#pragma once

#include <limits>

#include "hegel/core.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for integers() generator.
     * @tparam T The integer type
     */
    template <typename T> struct IntegersParams {
        std::optional<T>
            min_value; ///< Minimum value (inclusive). Default: type minimum
        std::optional<T>
            max_value; ///< Maximum value (inclusive). Default: type maximum
    };

    /**
     * @brief Parameters for floats() generator.
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

    /// @cond INTERNAL
    // Concrete IGenerator<T> subclass produced by integers().
    template <typename T>
        requires std::is_integral_v<T>
    class IntegerGenerator : public IGenerator<T> {
      public:
        explicit IntegerGenerator(IntegersParams<T> params = {})
            : params_(std::move(params)) {
            T min_val =
                params_.min_value.value_or(std::numeric_limits<T>::min());
            T max_val =
                params_.max_value.value_or(std::numeric_limits<T>::max());
            if (min_val > max_val) {
                throw std::invalid_argument(
                    "Cannot have max_value < min_value");
            }
        }

        std::optional<BasicGenerator<T>> as_basic() const override {
            T min_val =
                params_.min_value.value_or(std::numeric_limits<T>::min());
            T max_val =
                params_.max_value.value_or(std::numeric_limits<T>::max());
            return BasicGenerator<T>{{{"type", "integer"},
                                      {"min_value", min_val},
                                      {"max_value", max_val}},
                                     &default_parse_raw<T>};
        }

      private:
        IntegersParams<T> params_;
    };

    // Concrete IGenerator<T> subclass produced by floats().
    template <typename T>
        requires std::is_floating_point_v<T>
    class FloatGenerator : public IGenerator<T> {
      public:
        explicit FloatGenerator(FloatsParams<T> params = {})
            : params_(std::move(params)) {
            bool has_min = params_.min_value.has_value();
            bool has_max = params_.max_value.has_value();
            bool nan = params_.allow_nan.value_or(!has_min && !has_max);
            bool inf = params_.allow_infinity.value_or(!has_min || !has_max);
            if (nan && (has_min || has_max)) {
                throw std::invalid_argument(
                    "Cannot have allow_nan=true with min_value or max_value");
            }
            if (has_min && has_max && *params_.min_value > *params_.max_value) {
                throw std::invalid_argument(
                    "Cannot have max_value < min_value");
            }
            if (inf && has_min && has_max) {
                throw std::invalid_argument(
                    "Cannot have allow_infinity=true with both min_value and "
                    "max_value");
            }
        }

        std::optional<BasicGenerator<T>> as_basic() const override {
            constexpr int width = sizeof(T) * 8;
            bool has_min = params_.min_value.has_value();
            bool has_max = params_.max_value.has_value();
            bool nan = params_.allow_nan.value_or(!has_min && !has_max);
            bool inf = params_.allow_infinity.value_or(!has_min || !has_max);

            hegel::internal::json::json schema = {
                {"type", "float"},
                {"exclude_min", params_.exclude_min},
                {"exclude_max", params_.exclude_max},
                {"allow_nan", nan},
                {"allow_infinity", inf},
                {"width", width}};
            if (params_.min_value)
                schema["min_value"] = *params_.min_value;
            if (params_.max_value)
                schema["max_value"] = *params_.max_value;
            return BasicGenerator<T>{std::move(schema), &default_parse_raw<T>};
        }

      private:
        FloatsParams<T> params_;
    };
    /// @endcond

    /// @name Numeric
    /// @{

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
        return Generator<T>(new IntegerGenerator<T>(std::move(params)));
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
        return Generator<T>(new FloatGenerator<T>(std::move(params)));
    }

    /// @}

} // namespace hegel::generators
