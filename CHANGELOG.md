# Changelog

## 0.2.6 - 2026-03-11

Fix socket setup to prevent child process from inheriting fd.

## 0.2.5 - 2026-03-11

Add argument validation.

## 0.2.4 - 2026-03-06

Add `generators::randoms`, for use with `std::random`.

## 0.2.3 - 2026-03-06

Add clang-tidy and fix resulting lints.

## 0.2.2 - 2026-03-03

Replace `gen.generate()` with `draw(gen)`.


## 0.2.1 - 2026-03-02

Better error message when calling `assume()` or `note()` outside of a test.

## 0.2.0 - 2026-02-27

- Renamed `hegel::strategies` namespace to `hegel::generators`, consolidating all strategy factory functions into the existing generators namespace.
- Renamed `flatmap()` to `flat_map()` for snake_case consistency with the rest of the API.
- Re-exported `assume()`, `note()`, and `stop_test()` into the `hegel` namespace so callers no longer need to qualify them as `hegel::internal::assume()` etc.

## 0.1.3 - 2026-02-26

Rename the `strategies` namespace to `generators`.

## 0.1.2 - 2026-02-26

Add support older `nlohmann/json` versions.

## 0.1.1 - 2026-02-24

Remove unecessary internal calls to `assume()`.
