RELEASE_TYPE: minor

- Renamed `hegel::strategies` namespace to `hegel::generators`, consolidating all strategy factory functions into the existing generators namespace.
- Renamed `flatmap()` to `flat_map()` for snake_case consistency with the rest of the API.
- Re-exported `assume()`, `note()`, and `stop_test()` into the `hegel` namespace so callers no longer need to qualify them as `hegel::internal::assume()` etc.
