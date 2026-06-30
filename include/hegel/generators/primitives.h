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
    // Concrete IGenerator<T> subclass produced by just(). The schema's
    // "value" field is a placeholder — the engine draws zero entropy for a
    // constant, so the client parser returns the locally captured value
    // regardless of what the engine echoes back. This means just() works
    // for any T without requiring T to be JSON-serializable.
    template <typename T> class JustGenerator : public IGenerator<T> {
      public:
        explicit JustGenerator(T value) {
            this->basic_.emplace(BasicGenerator<T>{
                {{"type", "constant"}, {"value", nullptr}},
                [v = std::move(value)](
                    const hegel::internal::json::json_raw_ref&) { return v; }});
        }
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
