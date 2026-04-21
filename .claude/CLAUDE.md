# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the C++ library for Hegel, a universal property-based testing protocol. The library communicates with a hegel server (powered by Hypothesis) via a binary protocol over Unix sockets to generate random test data and perform shrinking.

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

## Dependencies

- C++20 compiler
- CMake 3.14+
- reflect-cpp v0.22.0 (automatic schema generation via reflection)
- nlohmann/json v3.12.0 (JSON manipulation + CBOR serialization)
- Google Test (for unit tests)

## Architecture

### Execution Model

The library spawns the hegel server as a subprocess and connects to it as a client:
1. Client creates a socket path and spawns the hegel server
2. The hegel server binds to the socket and listens
3. Client connects
4. Version negotiation: client sends `"Hegel/1.0"`, server responds `"Ok"`
5. Control stream (0) receives `run_test`/`test_case`/`test_done` events
6. Data streams handle `generate`/`start_span`/`stop_span`/`mark_complete`

### Protocol

Binary packet protocol with CBOR payloads over Unix socket:
- 20-byte header: magic (`0x4845474C` / "HEGL"), CRC32, stream ID, message ID, payload length
- CBOR-encoded payloads (nlohmann::json's `to_cbor()`/`from_cbor()`)
- Stream multiplexing: control stream 0, client streams use odd IDs
- Reply bit (`1 << 31`) in message ID field distinguishes requests from responses

### Key Components

Public headers in `include/hegel/`:
- **`hegel.h`** - Main include, declares `hegel::test()` entry point
- **`test_case.h`** - TestCase class with `draw()`, `assume()`, `note()` methods passed to the test callback
- **`core.h`** - `IGenerator<T>`, `Generator<T>`, `BasicGenerator<T>` (schema + client-side parser bundle), `CompositeGenerator<T>`, `MappedGenerator<T, U>` with `map()`, `flat_map()`, `filter()` combinators
- **`settings.h`** - `Settings`, `Database`, `Verbosity` enum
- **`internal.h`** - `communicate_with_core()` and the `HegelReject` exception (internal only; users interact via `TestCase` methods)
- **`json.h` / `nlohmann_reader.h`** - JSON interop helpers (avoid including `<nlohmann/json.hpp>` from public headers; `test_no_nlohmann_include.cpp` enforces this)
- **`generators/`** - Strategy factory functions in `hegel::generators` namespace, split by category: `primitives.h`, `numeric.h`, `strings.h`, `collections.h`, `combinators.h`, `formats.h`, `builds.h`, `default.h` (type-directed derivation via reflect-cpp), `random.h`

Private implementation in `src/`:
- **`protocol.{h,cpp}`** - Binary packet protocol, `Connection`, `Stream` classes
- **`connection.{h,cpp}`** - Subprocess spawn + Unix socket lifecycle, low-level socket I/O
- **`test_case.{h,cpp}`** - Private `TestCaseData` struct (holds per-iteration runtime state) and the `TestCase` class method implementations
- **`json_impl.h`** - Internal nlohmann-backed JSON implementation (not exposed publicly)
- **`generators.cpp` / `hegel.cpp` / `json.cpp`** - implementations for the corresponding public headers

### Generator Pattern

Each generator concept has its own concrete `IGenerator<T>` subclass (`IntegerGenerator<T>`, `VectorsGenerator<T>`, `OneOfGenerator<T>`, `TextGenerator`, …). The subclass stores its configuration and implements `as_basic()`, `schema()`, and `do_draw()`.

`as_basic()` returns an optional `BasicGenerator<T>` — a bundle of `(schema, parse: json_raw_ref → T)`. The parse closure decouples the CBOR schema sent to the server from how the client turns the response into `T`. It's called on every `do_draw` (schemas are rebuilt each time; cheap in practice).

- **Basic (schema-backed)**: primitives (`integers`, `text`, `just`, ...) always return `Some`. Composites (`vectors`, `one_of`, `optional`, `tuples`, `variant`, ...) return `Some` iff all their inputs are basic — drawing then sends a single compound schema and the client parser walks the response per-element.
- **Function-backed fallback**: `filter`, `flat_map`, and user-supplied `compose` have no schema path. Composites with non-basic inputs fall back *inside their own `do_draw`* to client-side generation (multiple round-trips, driven by `booleans()`/`integers()` for index/gate draws).

`map(f)` is implemented by `MappedGenerator<T, U>`, which composes `f` into the source's `BasicGenerator::parse` when available, preserving the schema:

```cpp
auto squared = integers<int>({.min_value = 0}).map([](int x) { return x * x; });
// squared.as_basic() is still Some — schema is integer, parse is compose(square, get_int)
```

Composite classes (`VectorsGenerator`, `SetsGenerator`, `MapsGenerator`, `TuplesGenerator`, `OneOfGenerator`, `OptionalGenerator`, `VariantGenerator`) build their compound schema from their inputs' basic schemas and a parser that iterates the server response applying each element's parser in turn. `OneOfGenerator` and `VariantGenerator` tag each branch with an index so the client knows which parser to apply.

## Code Style

- **Formatting**: LLVM base style, 4-space indentation, left-aligned pointers (`int*`). Run `just format` before committing.
- **Headers**: Use `.h` extension (not `.hpp`)
- **Namespaces**: `hegel` for public API (including run configuration types like `Settings`, `Database`, `Verbosity`), `hegel::generators` for generators and strategies, `hegel::internal` for internals referenced in public headers, `hegel::impl::*` for purely private implementation
- **Includes**: Public headers use relative includes (`#include "settings.h"`), source files use angle brackets for both public (`<hegel/internal.h>`) and private (`<socket.h>`) headers
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

**Wrong use** - masking server/protocol errors:
```cpp
// BAD: silently swallows a server error as if it were bad test data
tc.assume(response.contains("result"));

// GOOD: surface the error so it can be diagnosed and fixed
if (!response.contains("result")) {
    throw std::runtime_error("Server response missing 'result' field");
}
```

Rules of thumb:
- Server returned an error or malformed response? Throw `std::runtime_error`.
- Caller passed invalid arguments (e.g. empty vector)? Throw `std::invalid_argument`.
- Generated test data doesn't meet a precondition? Use `tc.assume()`.
