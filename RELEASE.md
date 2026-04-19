RELEASE_TYPE: minor

Multiple improvements to make hegel-cpp more idiomatic to use.

* Rename `hegel::hegel` to `hegel::test`.
* Rename `HegelSettings` to `Settings`.
* Move settings objects into the main `hegel` namespace, from the `hegel::settings` namespace. Removed the `hegel::settings` namespace.
* Rename `from_function` to `compose`. `compose` no longer requires an explicit declaration of the return type, unless you want to override the inferred type.
* Rename `dictionaries` to `maps`.
* Remove the `nulls()` generator, which misleadingly returned `std::monostate`.
* The `just` and `sampled_from` generators now accept any object, not just serializable objects.
