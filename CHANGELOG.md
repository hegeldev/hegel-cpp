# Changelog

## 0.7.1 - 2026-07-10

This patch adds three `TestCase` methods and three generators.

New `TestCase` methods:

- `tc.reject()` rejects the current test case unconditionally. Unlike `tc.assume(false)`, it is marked `[[noreturn]]` so it can stand in for a value in a branch that cannot continue.

- `tc.target(score, label = "")` records a numeric observation for the engine's targeted-search phase to maximize. Higher scores are treated as more interesting, so the engine biases later test cases toward inputs that produced higher scores under the same label.

- `tc.repeat(body)` runs `body` in an engine-managed loop whose iteration count the engine chooses and shrinks, like any other drawn value.

```cpp
int total = 0;
tc.repeat([&] {
    total += tc.draw(gs::integers<int>({.min_value = 0, .max_value = 10}));
    if (total >= 50) throw std::runtime_error("too much");
});
```

New generators:

- `uuids()` produces canonical hyphenated UUID strings (e.g. `f47ac10b-58cc-4372-a567-0e02b2c3d479`). By default any version is generated. Pass `{.version = N}` to force an RFC 4122 version (1-5).

- `arrays<T, N>(element)` produces a fixed-size `std::array<T, N>`, drawing exactly `N` elements from the generator 
`element`. Unlike `vectors()`, the length is fixed at compile time, so `N` is given explicitly: `arrays<int, 3>(integers<int>())`.

- `deferred<T>()` creates a forward reference for recursive or mutually recursive generators. Call `.generator()` to obtain handles before the implementation is known, embed them in other generators, then call `.set(...)` once to install the implementation:

```cpp
struct Tree { int leaf; std::vector<Tree> children; };

auto tree = gs::deferred<Tree>();
auto leaf = gs::integers<int>().map([](int v) { return Tree{v, {}}; });
auto branch = gs::compose([tree](const hegel::TestCase& tc) {
    return Tree{0, {tc.draw(tree.generator()), tc.draw(tree.generator())}};
});
tree.set(gs::one_of<Tree>({leaf, branch}));
```

## 0.7.0 - 2026-07-09

This release changes the default value of `fullmatch` in `from_regex` from `false` to `true`.

## 0.6.3 - 2026-07-09

This patch bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.27.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.27.0) to [0.28.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.28.0).

## 0.6.2 - 2026-07-09

This patch adds the ability to reproduce a specific failing example from its
reproduction blob and a `print_blob` setting to toggle printing those blobs.

To replay a failure, annotate a `HEGEL_TEST` with `HEGEL_REPRODUCE_FAILURE`. 
The test replays that blob instead of generating new cases:

```cpp
HEGEL_REPRODUCE_FAILURE(my_property, "AAEAAAAACgEAAAAA")
HEGEL_TEST(my_property)(hegel::TestCase& tc) {
    int n = tc.draw(gs::integers<int>());
    if (n < 50) {
        throw std::runtime_error("fail");
    }
}
```

At least one blob is required. More than one blob can be added for bookkeeping but 
only the first is replayed. Delete the annotation to return to a normal run.

When calling `hegel::test` directly, pass the blobs as the third argument:

```cpp
hegel::test(my_property, {}, {"AAEAAAAACgEAAAAA"});
```

## 0.6.1 - 2026-07-07

This release adds `HEGEL_TEST`, the new recommended way to define a property test. The macro defines the test as a plain function you can invoke from `main()` or any test framework, and derives a database key from the defining file and test name, so failing examples are persisted to the example database and replayed first on later runs. Settings can be written inline after the test name and become the function's default argument. Settings passed when invoking the test replaces them for that run:

```cpp
HEGEL_TEST(addition_commutes, {.test_cases = 500})(hegel::TestCase& tc) {
    int x = tc.draw(gs::integers<int>());
    int y = tc.draw(gs::integers<int>());
    if (x + y != y + x) {
        throw std::runtime_error("addition is not commutative");
    }
}
```

Each `HEGEL_TEST` is also registered with the new `hegel::run_all_tests()`, which runs every test defined with the macro in a translation unit. It reports failures to stderr and returns 0 if everything passed (1 otherwise).

You can run the test above in two ways:

```cpp
int main() {
    return hegel::run_all_tests();
}
```
```cpp
int main() {
    addition_commutes();
    return 0;
}
```

There are four new settings:

- `database_key`: the key scoping which examples are stored in and replayed from the database. `HEGEL_TEST` fills it in automatically. Set it yourself when calling `hegel::test()` directly.
- `phases`: which phases of the run to enable (`Phase::Explicit`, `Reuse`, `Generate`, `Target`, `Shrink`)
- `mode`: `Mode::TestRun` (the default) or `Mode::SingleTestCase`, which produces one test case and stops with no shrinking intended for long running tests
- `backend`: the engine's randomness source. `Backend::Auto` (the default), `Backend::Default` (seeded PRNG), or `Backend::Urandom` (fresh entropy per draw, intended for Antithesis)

## 0.6.0 - 2026-07-07

This release bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.23.2](https://github.com/hegeldev/hegel-rust/releases/tag/v0.23.2) to [0.27.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.27.0) and migrates the library to its reworked C ABI.

The CBOR request/response protocol has been removed, so the `HEGEL_PROTOCOL_DEBUG` environment variable and the Debug-verbosity `REQUEST:`/`RESPONSE:` dump have been removed. `Verbosity::Debug` now enables the engine's own shrinker tracing.

## 0.5.0 - 2026-07-02

This release adds the `report_multiple_failures` setting and reworks how
`hegel::test` reports failures.

With `report_multiple_failures` set to `true`, the engine keeps generating
after the first failure to surface additional distinct bugs. The setting 
defaults to `false`.

A test with a single failing example now re-raises the test's own exception
instead of `std::runtime_error`.

Foreign (non-C++) exceptions escaping a test body are now reported as a
`std::runtime_error` instead of undefined behavior.

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
