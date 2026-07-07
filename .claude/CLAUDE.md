# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the C++ library for Hegel, a universal property-based testing protocol. The library drives Hegel's native engine (libhegel, the Hypothesis-derived engine from hegel-rust) **in-process** through its C ABI to generate random test data and perform shrinking. There is no server process: libhegel is a small prebuilt shared library that the build downloads for the host platform.

## Build & Test Commands

```bash
just test     # cmake -B build && cmake --build build && ctest
just check    # build + test + format check (all CI checks)
just format   # clang-format on all .cpp/.h files
just docs     # Build Doxygen documentation
```

Run a single test:
```bash
cmake -B build && cmake --build build
ctest --test-dir build -R test_name
```

## Comments

- After making a change, do not describe what code was there previously and why 
the code was changed.
- Do not mention Hypothesis, or any other Hegel library, even when the user prompts
you to port a feature from there.
- Comments should not duplicate the code.

## Dependencies

- C++20 compiler by default. The only hard C++20 dependency is reflect-cpp (used by `default_generator`). Configure with `-DHEGEL_REFLECTION=OFF` to drop reflect-cpp and build/consume at C++17 — `default_generator` and automatic struct parsing become unavailable, but everything else works. The feature is gated by the `HEGEL_HAS_REFLECTION` macro (set from the CMake option; see `include/hegel/config.h`). Designated-initializer params (`integers<int>({.min_value = 0})`) then rely on a GCC/Clang C++17 extension.
- CMake 3.14+
- libhegel (Hegel's native engine) — a prebuilt shared library downloaded at configure time by `cmake/libhegel.cmake` from the hegel-rust GitHub release, verified against its published SHA-256, and linked. Override with `-DHEGEL_LIBHEGEL_LIBRARY=/path/to/libhegel_c.<ext>`. The vendored C ABI header lives at `libhegel/hegel.h`. Keep the version and hashes in `nix/flake.nix` in sync.
- reflect-cpp v0.22.0 (type-directed generator derivation via reflection)
- Google Test (for unit tests)

## Architecture

### Execution Model

The library calls libhegel's C ABI (`hegel_*` functions) directly, in-process — no subprocess, no socket. `hegel::test()` (`src/hegel.cpp`) drives the run:
1. Get this thread's error-reporting context (`impl::thread_context()`, one `hegel_context_t` per thread, mirroring hegel-rust) and a settings handle (`hegel_settings_new`); map `hegel::Settings` onto `hegel_settings_set_*`.
2. `hegel_run_start` starts the engine on a worker thread inside libhegel.
3. Loop `hegel_next_test_case` until it yields NULL; run the user body for each case and `hegel_mark_complete` it (VALID / INVALID / OVERRUN / INTERESTING).
4. `hegel_run_result` reports passed / failed / errored. On failure, each counterexample blob is replayed via `hegel_test_case_from_blob` to reproduce the user's notes and the failing exception message.

### Draw path

A `draw()` calls libhegel's typed draw primitives (`hegel_generate_integer`, `hegel_generate_float`, `hegel_generate_boolean`, `hegel_generate_bytes`, `hegel_generate_string`, `hegel_generate_date`/`_time`/`_datetime`, `hegel_generate_ipv4`/`_ipv6`) directly — there is no schema or serialization layer. The template-visible primitives (`draw_integer`, `draw_integer_unsigned`, `draw_float`, `draw_boolean`, spans, collections) are declared in `include/hegel/internal.h` and implemented in `src/engine.cpp`; the string/bytes/date draws used only by `src/generators.cpp` live in `src/engine.h` (`hegel::impl`).
- Integer ranges that don't fit in `int64_t` (e.g. `integers<uint64_t>()`) go through `hegel_generate_integer_big` with two's-complement little-endian bound buffers.
- String-family generators (`text`, `characters`, `from_regex`, `emails`, `urls`, `domains`) build a validated, immutable `hegel_string_generator_t` handle once at generator construction and draw through it with `hegel_generate_string`.
- `HEGEL_E_STOP_TEST` → `HegelStopTest` (case marked OVERRUN); `HEGEL_E_ASSUME` → `HegelReject` (INVALID); other non-OK codes throw `std::runtime_error` with `hegel_context_last_error`.

### Key Components

Public headers in `include/hegel/`:
- **`hegel.h`** - Main include, declares `hegel::test()` entry point
- **`test_case.h`** - TestCase class with `draw()`, `assume()`, `note()` methods passed to the test callback
- **`core.h`** - `IGenerator<T>`, `Generator<T>`, `BasicGenerator<T>` (schema + client-side parser bundle), `CompositeGenerator<T>`, `MappedGenerator<T, U>` with `map()`, `flat_map()`, `filter()` combinators
- **`settings.h`** - `Settings`, `Database`, `Verbosity` enum
- **`internal.h`** - The typed draw primitives (`draw_integer`, `draw_float`, `draw_boolean`, spans, collections), `SpanLabel`, and the `HegelReject` / `HegelStopTest` exceptions (internal only; users interact via `TestCase` methods)
- **`generators/`** - Strategy factory functions in `hegel::generators` namespace, split by category: `primitives.h`, `numeric.h`, `strings.h`, `collections.h`, `combinators.h`, `formats.h`, `builds.h`, `default.h` (type-directed derivation via reflect-cpp), `random.h`

Private implementation in `src/`:
- **`engine.{h,cpp}`** - Wrappers over the libhegel C ABI: run-lifecycle helpers (`hegel::impl`), string-generator construction, and the draw-primitive implementations (including `Generated:` logging on the final replay / at Verbose+)
- **`test_case.{h,cpp}`** - Private `TestCaseData` struct (holds the borrowed `hegel_context_t*` / `hegel_test_case_t*` plus per-iteration state) and the `TestCase` method implementations
- **`generators.cpp` / `hegel.cpp`** - implementations for the corresponding public headers; `hegel.cpp` also holds the `hegel::test()` run loop
- **`cmake/libhegel.cmake`** - downloads/verifies/links libhegel and exposes the `hegel::libhegel` imported target; `libhegel/hegel.h` is the vendored C ABI header

### Generator Pattern

Each generator concept has its own concrete `IGenerator<T>` subclass (`IntegerGenerator<T>`, `VectorsGenerator<T>`, `OneOfGenerator<T>`, `TextGenerator`, …). The subclass stores its configuration (validated in the constructor) and implements `do_draw()`, composing the typed draw primitives the same way hegel-rust's own generator library does:

- **Primitives** map 1:1 onto a draw call (`integers` → `draw_integer`/`draw_integer_unsigned`, `floats` → `draw_float`, `booleans` → `draw_boolean(0.5)`). `just()` draws nothing and returns its captured value.
- **Collections** (`vectors`, `sets`, `maps`) open their span (`SpanLabel::List`/`Set`/`Map`), create an engine-managed collection (`new_collection`), and loop `collection_more`, drawing one element per iteration — the engine owns the size logic. Duplicates (sets, maps, `unique` vectors) are pushed back with `collection_reject` so the engine draws a replacement.
- **Branching** (`one_of`, `variant`) opens a `SpanLabel::OneOf` span and draws a branch index; `optional` opens `SpanLabel::Optional` and gates on `draw_boolean(0.5)`; `sampled_from` is a bare index draw with no span.
- **Combinators**: `map` wraps the source draw in a `Mapped` span; `flat_map` in a `FlatMap` span; `filter` retries up to 3 times inside a `Filter` span, closing it with `discard=true` on predicate failure, then rejects the test case.

Spans exist so the shrinker can reason about a group of draws as a unit; open one span per compound draw and close it exactly once (rust-style: leave it open if an exception unwinds — the runner marks the case complete anyway).

## Code Style

- **Formatting**: LLVM base style, 4-space indentation, left-aligned pointers (`int*`). Run `just format` before committing.
- **Headers**: Use `.h` extension (not `.hpp`)
- **Namespaces**: `hegel` for public API (including run configuration types like `Settings`, `Database`, `Verbosity`), `hegel::generators` for generators and strategies, `hegel::internal` for internals referenced in public headers, `hegel::impl::*` for purely private implementation
- **Includes**: Public headers use relative includes (`#include "settings.h"`), source files use angle brackets for both public (`<hegel/internal.h>`) and private (`<engine.h>`, `<test_case.h>`) headers
- **File organization**: Each focused `.cpp` has a corresponding `.h` in `src/`. Private headers live in `src/`, not `include/`
- **Public API surface**: Minimal. Only what users need goes in `include/hegel/`. Internal details hidden via `@cond INTERNAL` / `@endcond` in Doxygen
- **Parameter structs**: Designated initializers (C++20): `integers<int>({.min_value = 0})`
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
