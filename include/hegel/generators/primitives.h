#pragma once

/**
 * @file primitives.h
 * @brief Primitive generator functions: nulls, booleans, just
 */

#include <variant>

#include "hegel/core.h"

namespace hegel::generators {

    /// @name Primitive Strategies
    /// @{

    /**
     * @brief Generate null values (std::monostate).
     * @return Generator that always produces std::monostate{}.
     */
    Generator<std::monostate> nulls();

    /**
     * @brief Generate random boolean values.
     * @return Generator producing `true` or `false`.
     */
    Generator<bool> booleans();

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
            hegel::internal::json::json schema = {{"type", "constant"},
                                                  {"value", value}};
            return from_function<T>([value](TestCaseData*) { return value; },
                                    std::move(schema));
        } else {
            return from_function<T>([value](TestCaseData*) { return value; });
        }
    }

    /**
     * @brief Generate a constant string value (string literal convenience).
     * @param value The C-string to convert to std::string.
     * @return Generator that always produces std::string(value).
     */
    inline Generator<std::string> just(const char* value) {
        return just(std::string(value));
    }

    /// @}

} // namespace hegel::generators
