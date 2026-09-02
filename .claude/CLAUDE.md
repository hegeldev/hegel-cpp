# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the C++ library for Hegel, a universal property-based testing protocol. The library drives Hegel's native engine (libhegel, the Hypothesis-derived engine from hegel-rust) **in-process** through its C ABI to generate random test data and perform shrinking. There is no server process: libhegel is a small prebuilt shared library that the build downloads for the host platform.

## Build & Test Commands

```bash
just test     # alias for check-tests: cmake -B build && cmake --build build && ctest --test-dir build/tests
just check    # check-lint check-tests check-docs check-coverage (the required CI checks)
just format   # clang-format on all .cpp/.h files
just docs     # build Doxygen documentation and open it
```

Run a single test:
```bash
cmake -B build && cmake --build build
ctest --test-dir build/tests -R test_name
```

Other recipes worth knowing before touching build/CI files:
- `just check-sanitizers` — separate `address,undefined` and `thread` builds under `build/san-*`; LSan suppressions at `tests/lsan-suppressions.txt`.
- `just check-tidy` — clang-tidy over `src/`, warnings-as-errors.
- `just check-consumer MODE` (`subdirectory` / `fetchcontent` / `install` / `tests_on`) and `just check-consumer-all` — build the library the way a downstream project would, not part of `just check`.
- `just check-cxx17` — builds and installs with `HEGEL_REFLECTION=OFF`, then builds/runs a standalone C++17 consumer.
- CMake options: `HEGEL_BUILD_TESTS`, `HEGEL_BUILD_DOCS` (OFF), `HEGEL_COVERAGE` (OFF), `HEGEL_REFLECTION` (ON), `HEGEL_THROW_SITE` (ON; OFF for statically linked C++ runtimes), plus `HEGEL_SANITIZE`, `HEGEL_LIBHEGEL_VERSION`/`HEGEL_LIBHEGEL_LIBRARY`, and `tests/CMakeLists.txt`'s `HEGEL_APPROVAL_TESTS` (ON).

## VERY IMPORTANT: Comments

- Adhere to ASD-STE100 Simplified Technical English
- After making a change, do not describe what code was there previously and why 
the code was changed.
- Do not mention Hypothesis, or any other Hegel library, even when the user prompts
you to port a feature from there.
- Comments should not duplicate the code.
- Adhere to ASD-STE100 Simplified Technical English

## Dependencies

- C++20 compiler by default. The only hard C++20 dependency is reflect-cpp (used by `default_generator`). Configure with `-DHEGEL_REFLECTION=OFF` to drop reflect-cpp and build/consume at C++17 — `default_generator` and automatic struct parsing become unavailable, but everything else works. The feature is gated by the `HEGEL_HAS_REFLECTION` macro (set from the CMake option; see `include/hegel/config.h`). Designated-initializer params (`integers<int>({.min_value = 0})`) then rely on a GCC/Clang C++17 extension.
- CMake 3.14+
- libhegel (Hegel's native engine) — a prebuilt shared library downloaded at configure time by `cmake/libhegel.cmake` from the hegel-rust GitHub release, verified against its published SHA-256, and linked. Override with `-DHEGEL_LIBHEGEL_LIBRARY=/path/to/libhegel_c.<ext>`. The vendored C ABI header lives at `libhegel/hegel.h`. Its version is bumped by a bot (`.github/workflows/bump-hegel-rust.yml` running `.github/scripts/bump_hegel_rust.py`) that rewrites `cmake/libhegel.cmake`, `libhegel/hegel.h`, and `nix/flake.nix` together in one PR — do not hand-bump it.
- reflect-cpp v0.22.0 (type-directed generator derivation via reflection). Version pinned in `CMakeLists.txt`; keep it in sync with `nix/flake.nix`'s `fetchDeps.reflectcpp`.
- Google Test v1.14.0 (for unit tests)
- ApprovalTests.cpp v.10.13.0 (test-only; snapshot tests in `tests/approvals/`). Fetched at configure time — keep the version in `tests/CMakeLists.txt` and `nix/flake.nix`'s `fetchDeps.approvaltests` in sync. `-DHEGEL_APPROVAL_TESTS=OFF` skips the fetch and the snapshot suites for offline builds. A mismatch writes a sibling `*.received.txt`; accept it by rerunning the test binary with `APPROVAL_TESTS_USE_REPORTER=AutoApproveReporter`.
- Coverage gate (`scripts/check-coverage.py`, run by `just check-coverage`): every non-excluded line in `src/` and `include/hegel/` must be covered, and lines hidden via `// GCOVR_EXCL_*` markers must not exceed the ratchet in `.github/coverage-ratchet.json`. Only a human raises the ratchet; it auto-tightens when the excluded count drops.
- Releases: a PR touching library behavior needs a `RELEASE.md` at the repo root (`RELEASE_TYPE: major|minor|patch` + changelog body, see the `changelog` skill) or the `skip release` label, or `check-release.yml` fails the PR. Merging to `main` auto-cuts a release and auto-publishes Doxygen docs.

## Architecture

### Execution Model

The library calls libhegel's C ABI (`hegel_*` functions) directly, in-process — no subprocess, no socket. `hegel::test()` (`src/hegel.cpp`) drives the run:
1. Get this thread's error-reporting context (`impl::thread_context()`, one `hegel_context_t` per thread, mirroring hegel-rust) and a settings handle (`hegel_settings_new`); map `hegel::Settings` onto `hegel_settings_set_*`.
2. `hegel_run_start` builds the run; no engine work happens yet.
3. Loop `hegel_next_test_case` until it yields NULL; run the user body for each case and `hegel_mark_complete` it (VALID / INVALID / OVERRUN / INTERESTING). libhegel has no background thread: all engine work (generation, mutation, shrinking) happens on the calling thread inside `hegel_next_test_case`.
4. `hegel_run_result` reports passed / failed / errored. On failure, each counterexample blob is replayed via `hegel_test_case_from_blob` to reproduce the user's notes and the failing exception message.
5. The replay is wrapped in a framed report: a header naming the test and its source line, a `Falsified after N test cases (M discarded):` line, the indented body of drawn values and notes, `Exception: <type>: <message>`, and a `rerun with:` line. Counting stops at the first failing case, so the number says how many cases it took to find the bug rather than how much the shrinker did. `hegel::test()` has an overload taking a `TestLocation`; `internal::test_from_macro` is what `HEGEL_TEST` expands to, and only it prints the `HEGEL_REPRODUCE_FAILURE` form of the rerun line.

Hegel runs no tests for you: `HEGEL_TEST` defines a function the user calls from `main()`, and there is no registry. With a test framework, the property goes inside one of its tests and calls `hegel::test()`.

### Test-framework integration

`internal::FrameworkHooks` (declared in `include/hegel/hegel.h`, stored in `src/hegel.cpp`) is how a framework takes part in a run: `current_test_name()` names the test that is running, and `run_case()` wraps each body invocation. `include/hegel/gtest.h` installs the GoogleTest pair from a namespace-scope variable initializer, and `hegel.h` includes it when `GTEST_TEST` is already defined. There is deliberately no opt-out: an assertion Hegel does not see reads as a passing test case.

- `run_case` runs the body under a `ScopedFakeTestPartResultReporter`, so the assertions a case fails do not reach GoogleTest, and raises them together as `hegel::GTestFailure`. Without this, an `ASSERT_*`/`EXPECT_*` failure only records and returns, which Hegel reads as a passing case.
- A failure's origin (what the engine groups bugs by) is the exception's demangled type plus the site it was thrown from, unless the exception also derives from `hegel::FailureOrigin`, in which case `run_body` takes `failure_origin()` instead. `GTestFailure` returns the positions of the assertions the case failed — never their messages, which hold generated values and would make every failing case its own bug.

### Throw sites

A bare `throw` carries no location, and the stack unwinds before `run_body` catches, so `src/hegel.cpp` defines its own `__cxa_throw` and forwards to the real one through `dlsym(RTLD_NEXT, ...)`. It records `__builtin_return_address(0)` in a `thread_local`, which `last_throw_site()` turns into `<binary>+0x<offset from its load address>` via `dladdr`. The offset is what tells two `throw`s apart; it is load-address independent, so the string repeats run to run under ASLR. Symbol names are deliberately not used: ELF `dladdr` reads only the dynamic symbol table, which cannot name a lambda or an internal function. Lookups are cached per thread because shrinking re-runs the failing body many times.

Hegel is a static library, so its definition lands in the test executable and wins for both ELF and Mach-O — provided the C++ runtime is a shared library. A statically linked runtime (e.g. `-static-libstdc++`, or a toolchain with `libc++abi.a`) defines `__cxa_throw` in an object the link always pulls in, and the two definitions collide; `HEGEL_THROW_SITE=OFF` sets `HEGEL_HAS_THROW_SITE=0` to compile the wrapper out for such toolchains. Where `RTLD_NEXT` is missing the whole thing compiles out the same way. Either way origins fall back to the type alone. `tests/subject_main.cpp`'s `throw_sites` scenario checks the grouping from a fresh process, which is what proves it survives ASLR.
- `Settings::database_key` defaults to `"<file>::<name>"`. The file comes from `__builtin_FILE()` in a default argument of `hegel::test()`, so it names the call site; the name is `current_test_name()` (`Suite.Name`) or `__builtin_FUNCTION()`. A disabled database gets no derived key — the key takes part in generation, so deriving one there would change what a run produces.
- `hegel::test()` builds a `TestLocation` only from `current_test_name()`, never from `__builtin_FUNCTION()`: a function name (often `operator()`) says too little to head a report with. Reports Hegel names itself carry an absolute path and the line of the call, so the snapshot suites scrub the header with `scrub_report()` (`tests/common/approvals.h`); a report named by an explicit `TestLocation` keeps its fixed position and uses `scrub_blob()`.

### Draw path

A `draw()` calls libhegel's typed draw primitives (`hegel_generate_integer`, `hegel_generate_float`, `hegel_generate_boolean`, `hegel_generate_bytes`, `hegel_generate_string`, `hegel_generate_date`/`_time`/`_datetime`, `hegel_generate_ipv4`/`_ipv6`, `hegel_generate_uuid`) directly — there is no schema or serialization layer. The template-visible primitives (`draw_integer`, `draw_integer_unsigned`, `draw_float`, `draw_boolean`, spans, collections) are declared in `include/hegel/internal.h` and implemented in `src/engine.cpp`; the string/bytes/date draws used only by `src/generators.cpp` live in `src/engine.h` (`hegel::impl`).
- Integer ranges that don't fit in `int64_t` (e.g. `integers<uint64_t>()`) go through `hegel_generate_integer_big` with two's-complement little-endian bound buffers.
- String-family generators (`text`, `characters`, `from_regex`, `emails`, `urls`, `domains`) build a validated, immutable `hegel_string_generator_t` handle once at generator construction and draw through it with `hegel_generate_string`.
- `HEGEL_E_STOP_TEST` → `HegelStopTest` (case marked OVERRUN); `HEGEL_E_ASSUME` → `HegelReject` (INVALID); other non-OK codes throw `std::runtime_error` with `hegel_context_last_error`.

### Key Components

Public headers in `include/hegel/`:
- **`hegel.h`** - Main include, declares `hegel::test()` entry point
- **`gtest.h`** - GoogleTest integration (header-only): `hegel::GTestFailure` and the `FrameworkHooks` it installs
- **`test_case.h`** - TestCase class (move-only) with `draw()`, `assume()`, `reject()`, `target()`, `repeat()`, `note()`, `clone()`, `spawn()` (returns a move-only `Worker<T>` for threaded drawing) methods passed to the test callback
- **`core.h`** - `IGenerator<T>`, `Generator<T>` with `map()`, `flat_map()`, `filter()` combinators, `CompositeGenerator<T>`, `MappedGenerator<T, U>`, and `compose()` to build a generator from an imperative `TestCase`-drawing lambda
- **`settings.h`** - `Settings`, `Database`, `Verbosity` enum
- **`internal.h`** - The typed draw primitives (`draw_integer`, `draw_float`, `draw_boolean`, spans, collections), the owning handles over the engine's compound-draw objects (`Collection`, and the `PoolHandle` / `StateMachineHandle` backing `stateful.h`, each of which frees its handle in its destructor), `SpanLabel`, `NoteIndentScope`, `DrawLogScope`, and the `HegelReject` / `HegelStopTest` exceptions (internal only; users interact via `TestCase` methods and the macros)
- **`generators/`** - Strategy factory functions in `hegel::generators` namespace, split by category: `primitives.h`, `numeric.h`, `strings.h`, `collections.h`, `combinators.h`, `formats.h`, `builds.h`, `default.h` (type-directed derivation via reflect-cpp), `random.h`, `stateful.h` (model-based/stateful testing — see Stateful testing below)

Private implementation in `src/`:
- **`engine.{h,cpp}`** - Wrappers over the libhegel C ABI: run-lifecycle helpers (`hegel::impl`), string-generator construction, and the draw-primitive implementations
- **`test_case.{h,cpp}`** - Private `TestCaseData` struct (owns the `hegel_test_case_t*` — freed in its destructor — plus per-iteration state; the error-reporting context is per-thread via `impl::thread_context()`) and the `TestCase` method implementations, including `DrawLogScope` — each outermost `TestCase::draw` prints its composed value as a C++ declaration (`auto <name> = <value>;`, rendered by `include/hegel/repr.h`) on the final replay / at Verbose+
- **`generators.cpp` / `hegel.cpp`** - implementations for the corresponding public headers; `hegel.cpp` also holds the `hegel::test()` run loop and the failure report
- **`cmake/libhegel.cmake`** - downloads/verifies/links libhegel and exposes the `hegel::libhegel` imported target; `libhegel/hegel.h` is the vendored C ABI header

### Generator Pattern

Each generator concept has its own concrete `IGenerator<T>` subclass (`IntegerGenerator<T>`, `VectorsGenerator<T>`, `OneOfGenerator<T>`, `TextGenerator`, …). The subclass stores its configuration (validated in the constructor) and implements `do_draw()`, composing the typed draw primitives the same way hegel-rust's own generator library does:

- **Primitives** map 1:1 onto a draw call (`integers` → `draw_integer`/`draw_integer_unsigned`, `floats` → `draw_float`, `booleans` → `draw_boolean(0.5)`). `just()` draws nothing and returns its captured value.
- **Collections** (`vectors`, `sets`, `maps`) open their span (`SpanLabel::List`/`Set`/`Map`), create an engine-managed collection (`Collection`), and loop over its `more()`, drawing one element per iteration — the engine owns the size logic. Duplicates (sets, maps, `unique` vectors) are pushed back with `reject()` so the engine draws a replacement.
- **Branching** (`one_of`, `variant`) opens a `SpanLabel::OneOf` span and draws a branch index; `optional` opens `SpanLabel::Optional` and gates on `draw_boolean(0.5)`; `sampled_from` is a bare index draw with no span.
- **Combinators**: `map` wraps the source draw in a `Mapped` span; `flat_map` in a `FlatMap` span; `filter` retries up to 3 times inside a `Filter` span, closing it with `discard=true` on predicate failure, then rejects the test case.

Spans exist so the shrinker can reason about a group of draws as a unit; open one span per compound draw and close it exactly once (rust-style: leave it open if an exception unwinds — the runner marks the case complete anyway).

### Stateful testing

`hegel::stateful` (`include/hegel/generators/stateful.h`, header-only, no `src/stateful.cpp`) is model-based state-machine testing, built entirely from the primitives above rather than a separate execution path. A user derives `StateMachine<Derived, State>` (CRTP) holding a `State` member, returning `std::vector<Rule<Derived>>` from `rules()` and (optionally) `std::vector<Invariant<Derived>>` from `invariants()`. A `Rule<T>` pairs a name with a step that draws from the `TestCase` and mutates the machine; an `Invariant<T>` is a named predicate that must throw to signal violation, checked before the first step and after every step that completes without an `assume()` rejection. `Pool<T>` lets one rule's output feed another rule's input as an ordinary `tc.draw(...)` (via `values_consumed`/`values_reusable` wrapping the pool in a `VariablesGenerator<T>`); it is non-copyable/non-movable and keeps a client-side map in sync with the engine's own pool state, throwing if they diverge.

`stateful::run()` opens `SpanLabel::StatefulRule` around each rule application so the shrinker can delete or reorder a whole step as a unit, and draws which rule fires via `hegel_state_machine_next_rule`. The engine owns the stepping lifecycle: each `next_rule` call first makes the stop decision itself, and writes `HEGEL_STATE_MACHINE_DONE` (mirrored as `internal::state_machine_done`) when the sequence ends — the loop breaks there, leaving the final span open for the engine to freeze. Every case runs at least one step and at most `Settings::stateful_step_count` (default 50, applied via `hegel_settings_set_stateful_step_count`; the engine rejects values below 1). Replay (`Mode::SingleTestCase`) is unbounded engine-side: a failing sequence reruns for real until a rule throws or an invariant fails, not until the step budget expires. A step that hits `assume()` closes its span with `discard=true` and reports itself through `hegel_state_machine_rule_rejected`, so it does not count against the step budget.

### Test Suite Organization

`tests/CMakeLists.txt` defines `hegel_add_test(NAME ... SOURCE ... [USE_UTILS] [INTERNAL] [APPROVALS])`, wrapping `gtest_discover_tests`; `APPROVALS` swaps `gtest_main` for `tests/common/approval_main.cpp` + `ApprovalTests::ApprovalTests`, and stores snapshots under `tests/approvals/`. `find_quality/test_*.cpp` and `shrink_quality/test_*.cpp` (one file per generator family) are ordinary GTest cases using `tests/common/utils.h`'s `find_any()`/`minimal()` helpers, which run `hegel::test()` against a predicate and assert the engine reaches (find_quality) or shrinks to (shrink_quality) a known value. `property_tests.cpp` holds worked-example properties, distinct from `test_generators_*.cpp`, which unit-test generator construction/validation one family per file. `subject_main.cpp` builds a standalone `subject` binary with argv-selected failure scenarios; `test_printing.cpp` forks it via `tests/common/subprocess.h` and approves its stderr through `tests/common/approvals.h`'s scrubbers, because failure-report formatting (throw-site stability under ASLR, multi-failure grouping) must be observed from a fresh process, not in-process. `tests/consumer/{subdirectory,fetchcontent,install,tests_on,cxx17}/` each exercise one way of consuming the library as a dependency, driven by `just check-consumer`/`check-consumer-all`. `tests/nix/` is a `find_package`-based smoke test of the *installed* package, driven by the Nix flake — distinct from `nix/` at the repo root, which is the flake itself.

## Code Style

- **Formatting**: LLVM base style, 4-space indentation, left-aligned pointers (`int*`). Run `just format` before committing.
- **Headers**: Use `.h` extension (not `.hpp`)
- **Namespaces**: `hegel` for public API (including run configuration types like `Settings`, `Database`, `Verbosity`), `hegel::generators` for generators and strategies, `hegel::internal` for internals referenced in public headers, `hegel::impl::*` for purely private implementation
- **Includes**: Public headers use relative includes (`#include "settings.h"`), source files use angle brackets for both public (`<hegel/internal.h>`) and private (`<engine.h>`, `<test_case.h>`) headers
- **File organization**: Each focused `.cpp` has a corresponding `.h` in `src/`. Private headers live in `src/`, not `include/`
- **Public API surface**: Minimal. Only what users need goes in `include/hegel/`. Internal details hidden via `@cond INTERNAL` / `@endcond` in Doxygen
- **Parameter structs**: Designated initializers (C++20): `integers<int>({.min_value = 0})`
- **Designated initializer order**: Always list designators in declaration order. Out-of-order designators are a hard error under GCC (Clang only accepts them as an extension), so a reordering that builds locally on Clang still breaks the GCC CI jobs.
- **Self-contained**: Prefer small standalone implementations over adding heavy dependencies

### Error Handling: `TestCase::assume()` vs exceptions

`tc.assume(condition)` (a method on the `TestCase` passed to a test callback) is **only** for filtering generated test data that doesn't meet preconditions. It signals to the framework that the current test case should be silently discarded (via `HegelReject`), not counted as a failure. It must never be used to handle errors in the library implementation itself.

**Correct use** - filtering generated values in tests:
```cpp
auto x = tc.draw(integers<int>());
tc.assume(x != std::numeric_limits<int32_t>::min());  // Skip edge case
```

**Wrong use** - masking engine errors:
```cpp
// BAD: silently swallows an engine error as if it were bad test data
tc.assume(rc == HEGEL_OK);

// GOOD: surface the error so it can be diagnosed and fixed
if (rc != HEGEL_OK) {
    throw std::runtime_error("hegel_generate_integer failed: " + last_error(ctx));
}
```

Rules of thumb:
- libhegel returned an unexpected error code? Throw `std::runtime_error`.
- Caller passed invalid arguments (e.g. empty vector)? Throw `std::invalid_argument`.
- Generated test data doesn't meet a precondition? Use `tc.assume()`.
