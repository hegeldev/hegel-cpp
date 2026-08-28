# Changelog

## 0.11.4 - 2026-08-14

This patch bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.32.3](https://github.com/hegeldev/hegel-rust/releases/tag/v0.32.3) to [0.32.5](https://github.com/hegeldev/hegel-rust/releases/tag/v0.32.5).

## 0.11.3 - 2026-08-11

This patch bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.32.2](https://github.com/hegeldev/hegel-rust/releases/tag/v0.32.2) to [0.32.3](https://github.com/hegeldev/hegel-rust/releases/tag/v0.32.3).

## 0.11.2 - 2026-08-10

This patch bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.30.5](https://github.com/hegeldev/hegel-rust/releases/tag/v0.30.5) to [0.32.2](https://github.com/hegeldev/hegel-rust/releases/tag/v0.32.2).

## 0.11.1 - 2026-08-04

This patch bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.29.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.29.0) to [0.30.5](https://github.com/hegeldev/hegel-rust/releases/tag/v0.30.5).

## 0.11.0 - 2026-08-04

This release replaces the Hegel test framework with an opt-in GTest integration.
`gtest_throw_on_failure` is no longer needed for a property to see a failed 
assertion. Users can still call existing Hegel tests as regular functions from 
`main()`.

 `HEGEL_DRAW`, `HEGEL_REQUIRE_EQUAL`, `HEGEL_REQUIRE`, `HEGEL_FAIL`, 
 `hegel::run_all_tests()`, and the test registry have been removed. 

Hegel can now distinguish between multiple exceptions of the same type. Their
locations in the test function is the differentiator by default. 
Derive exceptions from `hegel::FailureOrigin` to differentiate them some other
way.

## 0.10.0 - 2026-07-29

This release requires the values `HEGEL_REQUIRE_EQUAL` compares to be ones
Hegel can render or to have an `operator<<`, and fixes a bug where two
unrenderable values were always recognized as equal.

## 0.9.0 - 2026-07-28

This release overhauls what Hegel prints when a property fails, adds three
macros for stating what a test requires, and changes how a state machine
declares its state to accommodate state printing.

A failing run now produces a framed report like the one below:

```
--- Failure: sort_agrees_with_std_sort (sort_test.cpp:9) ----------------
Falsified after 8 test cases (0 discarded):

  auto vec1 = std::vector<int>{0, 0};

Exception: std::runtime_error: sort mismatch
rerun with: HEGEL_REPRODUCE_FAILURE(sort_agrees_with_std_sort, "AXicY2VgYGBkZOBiZEBhMAAAAd8AIQ==")
```

`Settings::print_blob` now defaults to `true`, so the `rerun with:` line
prints by default. A run that finds several distinct failures reports each in
its own numbered section.

`HEGEL_TEST` supplies the test's name and source line, so its failures name
themselves in the header. `hegel::test()` now optionally takes a `TestLocation`.

```cpp
hegel::test(my_body, {"my_property", __FILE__, __LINE__}, {.test_cases = 500});
```

This release also adds `HEGEL_REQUIRE`, `HEGEL_REQUIRE_EQUAL`, and
`HEGEL_FAIL` for stating properties and failures.

```cpp
HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
    HEGEL_DRAW(tc, x, gs::integers<int>());
    HEGEL_DRAW(tc, y, gs::integers<int>());
    HEGEL_REQUIRE_EQUAL(tc, x + y, y + x);
}
```

For an equality property prefer `HEGEL_REQUIRE_EQUAL`. Its report shows a
difference of the two values:

```
HEGEL_REQUIRE_EQUAL: values differ (- lhs / + rhs):
  Team{
    .name = std::string("a"),
    .scores = std::vector<int>{
      1,
-     2,
+     9,
      3,
    },
  }
```

Prefer all three over a raw `throw`. Hegel tells one bug from another by
where the failure came from, and a raw `throw` carries no source position, so 
every throw of one type in a run counts as a single bug under 
`Settings::report_multiple_failures`. Each macro invocation carries the position
of the line you wrote it on, so failures on different lines are distinct.

`StateMachine` now takes the state's type as a second template argument and 
holds the state itself, and its constructor takes the initial state. A failing 
stateful run prints that state before the first step and after every step that
runs to completion.

```cpp
struct Stack : hegel::stateful::StateMachine<Stack, std::vector<int>> {
    Stack() : StateMachine({.initial_state = {}}) {}

    std::vector<hegel::stateful::Rule<Stack>> rules() {
        return {hegel::stateful::Rule<Stack>(
            "push", [](hegel::TestCase& tc, Stack& m) {
                m.state.push_back(tc.draw(gs::integers<int>()));
            })};
    }
};
```

State printing can be disabled with `.print_state = false`.

```cpp
hegel::stateful::run(machine, tc, {.print_state = false});
```

## 0.8.0 - 2026-07-27

This release changes how drawn values are printed in failure replays and
verbose output. Hegel previously printed one `Generated: <value>` line per
primitive draw inside the engine. It now prints one line per user-level `tc.draw(...)`, 
with the final composed value rendered as a C++ declaration:

```
auto draw_1 = std::vector<int>{0, 0};
```

Types Hegel cannot render as an expression fall back to
their `operator<<` output, then to an `<unprintable Type>` placeholder.

This release also adds named draws. `tc.draw("x", gen)` prints the draw
under the given name, and the new `HEGEL_DRAW` macro binds a variable and
captures its name in one step:

```cpp
HEGEL_TEST(addition_commutes)(hegel::TestCase& tc) {
    HEGEL_DRAW(tc, x, gs::integers<int>());
    HEGEL_DRAW(tc, y, gs::integers<int>());
    if (x + y != y + x) throw std::runtime_error("not commutative");
}
// replay output: auto x = 10;
//                auto y = 3;
```

A name prints bare on every use. Pass `repeatable = true`
(`tc.draw("x", gen, true)`) to number repeated draws `x_1`, `x_2`, ...
for draws in loops. Unnamed draws print as `draw_1`, `draw_2`, ...
per test case. In stateful tests, each rule's draws print indented under
their `Step N:` header.

## 0.7.5 - 2026-07-22

This patch adds test-case cloning. `TestCase::clone()` forks an independent draw stream of the current test case. The clone draws from its own choice sequence but shares the case's outcome and budget. A single test case must not be drawn from concurrently.

`TestCase::spawn()` runs a callable on a clone in a new thread and returns a `hegel::Worker`. `Worker::join()` awaits it, returning the callable's result and re-raising any exception it threw. Join every worker before the test body returns.

```cpp
auto worker = tc.spawn([](hegel::TestCase& c) {
    return c.draw(gs::integers<int>());
});
auto mine = tc.draw(gs::integers<int>());
auto theirs = worker.join();
```

## 0.7.4 - 2026-07-18

This patch adds stateful testing under the `hegel::stateful` namespace. A stateful test drives the system under test through a sequence of randomly chosen actions applied to a state machine. When a sequence falsifies an invariant or throws, the engine shrinks it to a minimal failing sequence and replays it.

Derive a machine from `hegel::stateful::StateMachine<T>` and define its `rules()`, returning `hegel::stateful::Rule<T>` actions. Optionally override `invariants()` to add `hegel::stateful::Invariant<T>` predicates. Pass an instance to `hegel::stateful::run`. A rule's step mutates the machine in place. Invariants are evaluated before the first step and after every valid step.

```cpp
namespace gs = hegel::generators;

struct IntegerStack : hegel::stateful::StateMachine<IntegerStack> {
    std::vector<int> stack;

    std::vector<hegel::stateful::Rule<IntegerStack>> rules() {
        return {
            hegel::stateful::Rule<IntegerStack>(
                "push", [](hegel::TestCase& tc, IntegerStack& m) {
                    m.stack.push_back(tc.draw(
                        gs::integers<int>({.min_value = 0, .max_value = 100})));
                }),
            hegel::stateful::Rule<IntegerStack>(
                "pop", [](hegel::TestCase& tc, IntegerStack& m) {
                    tc.assume(!m.stack.empty());
                    m.stack.pop_back();
                }),
        };
    }
};

hegel::test([](hegel::TestCase& tc) {
    IntegerStack machine;
    hegel::stateful::run(machine, tc);
});
```

## 0.7.3 - 2026-07-16

This patch adds pools for storing previously generated values. It also adds two
generators, `values_reusable` and `values_consumed`, for drawing previously generated
values with and without replacement, respectively.

Pools will be more useful once stateful testing is added in a later release.

## 0.7.2 - 2026-07-15

This patch bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.28.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.28.0) to [0.29.0](https://github.com/hegeldev/hegel-rust/releases/tag/v0.29.0).

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
