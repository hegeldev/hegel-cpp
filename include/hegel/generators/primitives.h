#pragma once

#include "hegel/core.h"

namespace hegel::generators {

    /// @name Primitives
    /// @{

    /**
     * @brief Generate random boolean values.
     * @return Generator producing `true` or `false`.
     */
    Generator<bool> booleans();

    /// @cond INTERNAL
    // Concrete IGenerator<T> subclass produced by just(). A constant draws
    // zero entropy, so it returns the captured value without touching the
    // engine at all.
    template <typename T> class JustGenerator : public IGenerator<T> {
      public:
        explicit JustGenerator(T value) : value_(std::move(value)) {}

        T do_draw(const TestCase&) const override { return value_; }

      private:
        T value_;
    };
    /// @endcond

    /**
     * @brief Generate a constant value.
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
        return Generator<T>(new JustGenerator<T>(std::move(value)));
    }

    /**
     * @brief Overload for `just` so that `just("a string literal")` has type
     * `Generator<std::string>` rather than `Generator<const char*>`.
     * @param value The constant value to generator
     * @return Generator that always produces value
     */
    inline Generator<std::string> just(const char* value) {
        return just(std::string(value));
    }

    /// @}

} // namespace hegel::generators
