#pragma once

// Compile-time feature configuration. Not part of the documented API surface.

// HEGEL_HAS_REFLECTION — whether the reflect-cpp powered features
// (default_generator and automatic parsing of reflectable structs) are
// available. The CMake build defines it: 1 with HEGEL_REFLECTION=ON (default,
// requires C++20), 0 otherwise. When unset (headers used outside the CMake
// target) it falls back to whether the compiler advertises C++20 concepts.
// HEGEL_REFLECTION=OFF drops reflect-cpp so the library is consumable from
// C++17; everything except default_generator still works.
#ifndef HEGEL_HAS_REFLECTION
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define HEGEL_HAS_REFLECTION 1
#else
#define HEGEL_HAS_REFLECTION 0
#endif
#endif

// HEGEL_REQUIRES — a requires-clause under C++20, nothing under C++17.
// Constrained templates pair it with a static_assert so misuse is still
// rejected with a clear message when concepts are unavailable.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define HEGEL_REQUIRES(...) requires(__VA_ARGS__)
#else
#define HEGEL_REQUIRES(...)
#endif

namespace hegel::internal {
    /// @cond INTERNAL
    /// Always-false dependent value, for `static_assert` in discarded
    /// `if constexpr` / `#if` branches.
    template <typename...> inline constexpr bool always_false_v = false;
    /// @endcond
} // namespace hegel::internal
