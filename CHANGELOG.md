# Changelog

## 0.3.4 - 2026-04-21

General documentation improvements

## 0.3.3 - 2026-04-20

Update and build our documentation to https://hegel.dev/.

## 0.3.2 - 2026-04-20

Update how we install uv and hegel-core to match https://hegel.dev/reference/installation.

## 0.3.1 - 2026-04-20

Support displaying user defined exceptions, health check failures, and flaky failures.

## 0.3.0 - 2026-04-20

Multiple improvements to make hegel-cpp more idiomatic to use.

* Rename `hegel::hegel` to `hegel::test`.
* Rename `HegelSettings` to `Settings`.
* Move settings objects into the main `hegel` namespace, from the `hegel::settings` namespace. Removed the `hegel::settings` namespace.
* Rename `from_function` to `compose`. `compose` no longer requires an explicit declaration of the return type, unless you want to override the inferred type.
* Rename `dictionaries` to `maps`.
* Remove the `nulls()` generator, which misleadingly returned `std::monostate`.
* The `just` and `sampled_from` generators now accept any object, not just serializable objects.

## 0.2.0 - 2026-04-16

Multiple refactors and renames, as we prepare for a more standard release.

## 0.1.0 - 2026-04-15

Initial release!
