#pragma once

/**
 * @file builds.h
 * @brief Object construction generator functions: builds, builds_agg, field
 */

#include <tuple>

#include "hegel/core.h"

namespace hegel::generators {

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

        return from_function<T>([gen_tuple](const TestCase& tc) {
            return std::apply([&tc](auto&... g) { return T(g.do_draw(tc)...); },
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

        return from_function<T>([gen_tuple](const TestCase& tc) mutable {
            T result{};
            std::apply(
                [&result, &tc](auto&... fs) {
                    ((result.*
                          (std::remove_reference_t<decltype(fs)>::member_ptr) =
                          fs.generator.do_draw(tc)),
                     ...);
                },
                gen_tuple);
            return result;
        });
    }

    /// @}

} // namespace hegel::generators
