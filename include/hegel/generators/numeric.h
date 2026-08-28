#pragma once

#include <cstdint>
#include <limits>

#include "hegel/config.h"
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
    HEGEL_REQUIRES(std::is_integral_v<T>)
    class IntegerGenerator : public IGenerator<T> {
        static_assert(std::is_integral_v<T>,
                      "integers<T> requires an integral type T");

      public:
        explicit IntegerGenerator(const IntegersParams<T>& params = {})
            : min_(params.min_value.value_or(std::numeric_limits<T>::min())),
              max_(params.max_value.value_or(std::numeric_limits<T>::max())) {
            if (min_ > max_) {
                throw std::invalid_argument(
                    "Cannot have max_value < min_value");
            }
        }

        T do_draw(const TestCase& tc) const override {
            if constexpr (std::is_signed_v<T>) {
                return static_cast<T>(hegel::internal::draw_integer(
                    tc, static_cast<int64_t>(min_),
                    static_cast<int64_t>(max_)));
            } else {
                return static_cast<T>(hegel::internal::draw_integer_unsigned(
                    tc, static_cast<uint64_t>(min_),
                    static_cast<uint64_t>(max_)));
            }
        }

      private:
        T min_;
        T max_;
    };

    // Concrete IGenerator<T> subclass produced by floats(). Resolves the
    // engine draw parameters once at construction.
    template <typename T>
    HEGEL_REQUIRES(std::is_floating_point_v<T>)
    class FloatGenerator : public IGenerator<T> {
        static_assert(std::is_floating_point_v<T>,
                      "floats<T> requires a floating-point type T");

      public:
        explicit FloatGenerator(const FloatsParams<T>& params = {}) {
            bool has_min = params.min_value.has_value();
            bool has_max = params.max_value.has_value();
            allow_nan_ = params.allow_nan.value_or(!has_min && !has_max);
            allow_infinity_ =
                params.allow_infinity.value_or(!has_min || !has_max);
            if (allow_nan_ && (has_min || has_max)) {
                throw std::invalid_argument(
                    "Cannot have allow_nan=true with min_value or max_value");
            }
            if (has_min && has_max && *params.min_value > *params.max_value) {
                throw std::invalid_argument(
                    "Cannot have max_value < min_value");
            }
            if (allow_infinity_ && has_min && has_max) {
                throw std::invalid_argument(
                    "Cannot have allow_infinity=true with both min_value and "
                    "max_value");
            }

            // An unset bound is the widest the draw allows: infinite when
            // infinities may be drawn, else the type's finite extreme.
            min_ = has_min ? static_cast<double>(*params.min_value)
                   : allow_infinity_
                       ? -std::numeric_limits<double>::infinity()
                       : static_cast<double>(std::numeric_limits<T>::lowest());
            max_ = has_max ? static_cast<double>(*params.max_value)
                   : allow_infinity_
                       ? std::numeric_limits<double>::infinity()
                       : static_cast<double>(std::numeric_limits<T>::max());
            exclude_min_ = params.exclude_min;
            exclude_max_ = params.exclude_max;
        }

        T do_draw(const TestCase& tc) const override {
            constexpr uint32_t width = sizeof(T) * 8;
            return static_cast<T>(hegel::internal::draw_float(
                tc, width, min_, max_, allow_nan_, allow_infinity_,
                exclude_min_, exclude_max_,
                static_cast<double>(std::numeric_limits<T>::denorm_min())));
        }

      private:
        double min_;
        double max_;
        bool allow_nan_;
        bool allow_infinity_;
        bool exclude_min_;
        bool exclude_max_;
    };
    /// @endcond

    /// @name Numeric
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
    HEGEL_REQUIRES(std::is_integral_v<T>)
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
    HEGEL_REQUIRES(std::is_floating_point_v<T>)
    Generator<T> floats(FloatsParams<T> params = {}) {
        return Generator<T>(new FloatGenerator<T>(std::move(params)));
    }

    /// @}

} // namespace hegel::generators
