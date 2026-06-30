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
- libhegel (Hegel's native engine) — a prebuilt shared library downloaded at configure time by `cmake/libhegel.cmake` from the hegel-rust GitHub release, verified against its published SHA-256, and linked. Override with `-DHEGEL_LIBHEGEL_LIBRARY=/path/to/libhegel_c.<ext>`. The vendored C ABI header lives at `libhegel/hegel.h`.
- reflect-cpp v0.22.0 (automatic schema generation via reflection)
- nlohmann/json v3.12.0 (JSON manipulation + CBOR serialization)
- Google Test (for unit tests)

## Architecture

### Execution Model

The library calls libhegel's C ABI (`hegel_*` functions) directly, in-process — no subprocess, no socket. `hegel::test()` (`src/hegel.cpp`) drives the run:
1. Create a context + settings handle (`hegel_context_new`, `hegel_settings_new`); map `hegel::Settings` onto `hegel_settings_set_*`.
2. `hegel_run_start` starts the engine on a worker thread inside libhegel.
3. Loop `hegel_next_test_case` until it yields NULL; run the user body for each case and `hegel_mark_complete` it (VALID / INVALID / OVERRUN / INTERESTING).
4. `hegel_run_result` reports passed / failed / errored. On failure, each counterexample blob is replayed via `hegel_test_case_from_blob` to reproduce the user's notes and the failing exception message.

### Draw path

A `draw()` calls `internal::generate_from_schema(schema, tc)` (`src/engine.cpp`), which CBOR-encodes the generator's schema, calls `hegel_generate`, and CBOR-decodes the returned value:
- CBOR via nlohmann's `to_cbor()`/`from_cbor()` (`src/protocol.h`); WTF-8 hegel strings arrive as tagged binary (subtype 91) and are converted back to strings.
- `HEGEL_E_STOP_TEST` → `HegelStopTest` (case marked OVERRUN); `HEGEL_E_ASSUME` → `HegelReject` (INVALID); other non-OK codes throw `std::runtime_error` with `hegel_context_last_error`.

### Key Components

Public headers in `include/hegel/`:
- **`hegel.h`** - Main include, declares `hegel::test()` entry point
- **`test_case.h`** - TestCase class with `draw()`, `assume()`, `note()` methods passed to the test callback
- **`core.h`** - `IGenerator<T>`, `Generator<T>`, `BasicGenerator<T>` (schema + client-side parser bundle), `CompositeGenerator<T>`, `MappedGenerator<T, U>` with `map()`, `flat_map()`, `filter()` combinators
- **`settings.h`** - `Settings`, `Database`, `Verbosity` enum
- **`internal.h`** - `generate_from_schema()` and the `HegelReject` / `HegelStopTest` exceptions (internal only; users interact via `TestCase` methods)
- **`json.h` / `nlohmann_reader.h`** - JSON interop helpers (avoid including `<nlohmann/json.hpp>` from public headers; `test_no_nlohmann_include.cpp` enforces this)
- **`generators/`** - Strategy factory functions in `hegel::generators` namespace, split by category: `primitives.h`, `numeric.h`, `strings.h`, `collections.h`, `combinators.h`, `formats.h`, `builds.h`, `default.h` (type-directed derivation via reflect-cpp), `random.h`

Private implementation in `src/`:
- **`engine.{h,cpp}`** - Thin helpers over the libhegel C ABI: `last_error()` and the `generate_from_schema()` draw path (`hegel_generate`)
- **`protocol.{h,cpp}`** - CBOR encode/decode helpers (nlohmann-backed) + the protocol-debug flag. (The former binary packet/socket protocol is gone.)
- **`test_case.{h,cpp}`** - Private `TestCaseData` struct (holds the borrowed `hegel_context_t*` / `hegel_test_case_t*` plus per-iteration state) and the `TestCase` method implementations
- **`json_impl.h`** - Internal nlohmann-backed JSON implementation (not exposed publicly)
- **`generators.cpp` / `hegel.cpp` / `json.cpp`** - implementations for the corresponding public headers; `hegel.cpp` also holds the `hegel::test()` run loop
- **`cmake/libhegel.cmake`** - downloads/verifies/links libhegel and exposes the `hegel::libhegel` imported target; `libhegel/hegel.h` is the vendored C ABI header

### Generator Pattern

Each generator concept has its own concrete `IGenerator<T>` subclass (`IntegerGenerator<T>`, `VectorsGenerator<T>`, `OneOfGenerator<T>`, `TextGenerator`, …). The subclass stores its configuration and implements `as_basic()`, `schema()`, and `do_draw()`.

A schema-backed generator holds a `BasicGenerator<T>` — a bundle of `(schema, parse: json_raw_ref → T)`. The parse closure decouples the CBOR schema sent to the engine from how the client turns the response into `T`. Each generator builds this once **in its constructor** and stores it in `IGenerator<T>::basic_` (a protected `std::optional`); composites build theirs from their children's `basic()`. `do_draw()`/`schema()` and the composite fallbacks read `basic()`. Building the schema per draw (rather than once at construction) used to dominate shrink-heavy runs. Generators with no schema path (`filter`, `flat_map`, user `compose`) leave `basic_` empty and override `do_draw()`.

- **Basic (schema-backed)**: primitives (`integers`, `text`, `just`, ...) always return `Some`. Composites (`vectors`, `one_of`, `optional`, `tuples`, `variant`, ...) return `Some` iff all their inputs are basic — drawing then sends a single compound schema and the client parser walks the response per-element.
- **Function-backed fallback**: `filter`, `flat_map`, and user-supplied `compose` have no schema path. Composites with non-basic inputs fall back *inside their own `do_draw`* to client-side generation (multiple `hegel_generate` calls, driven by `booleans()`/`integers()` for index/gate draws).

`map(f)` is implemented by `MappedGenerator<T, U>`, which composes `f` into the source's `BasicGenerator::parse` when available, preserving the schema:

```cpp
auto squared = integers<int>({.min_value = 0}).map([](int x) { return x * x; });
// squared.as_basic() is still Some — schema is integer, parse is compose(square, get_int)
```

Composite classes (`VectorsGenerator`, `SetsGenerator`, `MapsGenerator`, `TuplesGenerator`, `OneOfGenerator`, `OptionalGenerator`, `VariantGenerator`) build their compound schema from their inputs' basic schemas and a parser that iterates the server response applying each element's parser in turn. `OneOfGenerator` and `VariantGenerator` tag each branch with an index so the client knows which parser to apply.

(The engine response is the value libhegel returns from `hegel_generate`; "round-trip" now means an in-process C ABI call, not a socket exchange.)

## Code Style

- **Formatting**: LLVM base style, 4-space indentation, left-aligned pointers (`int*`). Run `just format` before committing.
- **Headers**: Use `.h` extension (not `.hpp`)
- **Namespaces**: `hegel` for public API (including run configuration types like `Settings`, `Database`, `Verbosity`), `hegel::generators` for generators and strategies, `hegel::internal` for internals referenced in public headers, `hegel::impl::*` for purely private implementation
- **Includes**: Public headers use relative includes (`#include "settings.h"`), source files use angle brackets for both public (`<hegel/internal.h>`) and private (`<protocol.h>`, `<engine.h>`) headers
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

**Wrong use** - masking engine/protocol errors:
```cpp
// BAD: silently swallows an engine error as if it were bad test data
tc.assume(response.contains("result"));

// GOOD: surface the error so it can be diagnosed and fixed
if (!response.contains("result")) {
    throw std::runtime_error("Engine response missing 'result' field");
}
```

Rules of thumb:
- libhegel returned an error or malformed response? Throw `std::runtime_error`.
- Caller passed invalid arguments (e.g. empty vector)? Throw `std::invalid_argument`.
- Generated test data doesn't meet a precondition? Use `tc.assume()`.
