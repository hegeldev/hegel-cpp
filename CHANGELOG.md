# Changelog

## 0.4.0 - 2026-06-30

This release replaces the Python hegel-core engine with the `libhegel` Rust engine,
called in-process through its C ABI. There is no longer a subprocess, socket,
or wire protocol, and `uv` is no longer required.

The public generator and `hegel::test` API is unchanged.

This release also adds a C++17 build path. Configure with `-DHEGEL_REFLECTION=OFF`
to drop the reflect-cpp dependency and build at C++17. `default_generator` and
automatic struct derivation become unavailable, but every other generator and
combinator still works.

## 0.3.9 - 2026-05-20

This patch bumps our pinned hegel-core from [0.6.0](https://github.com/hegeldev/hegel-core/releases/tag/v0.6.0) to [0.9.1](https://github.com/hegeldev/hegel-core/releases/tag/v0.9.1).

## 0.3.8 - 2026-04-30

Internal refactor.

## 0.3.7 - 2026-04-29

Internal refactor of `one_of`.

## 0.3.6 - 2026-04-28

Bump our pinned [`hegel-core`](https://github.com/hegeldev/hegel-core) version from `0.4.0` to [`0.4.14`](https://github.com/hegeldev/hegel-core/releases/tag/v0.4.14).

## 0.3.5 - 2026-04-21

Fix our CMake integration with `FetchContent` or `add_subdirectory`, which previously errored.

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
