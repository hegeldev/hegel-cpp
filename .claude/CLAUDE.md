# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the C++ SDK for Hegel, a universal property-based testing framework. The SDK communicates with a hegeld server (powered by Hypothesis) via a binary protocol over Unix sockets to generate random test data and perform shrinking.

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

The SDK spawns `hegeld` as a subprocess and connects to it as a client:
1. SDK creates a socket path and spawns hegeld
2. hegeld binds to the socket and listens
3. SDK connects as client
4. Version negotiation: SDK sends `"Hegel/1.0"`, server responds `"Ok"`
5. Control channel (0) receives `run_test`/`test_case`/`test_done` events
6. Data channels handle `generate`/`start_span`/`stop_span`/`mark_complete`

### Protocol

Binary packet protocol with CBOR payloads over Unix socket:
- 20-byte header: magic (`0x4845474C` / "HEGL"), CRC32, channel ID, message ID, payload length
- CBOR-encoded payloads (nlohmann::json's `to_cbor()`/`from_cbor()`)
- Channel multiplexing: control channel 0, client channels use odd IDs
- Reply bit (`1 << 31`) in message ID field distinguishes requests from responses

### Key Components

- **`hegel.h`** - Main include, declares `hegel::hegel()` entry point
- **`options.h`** - HegelOptions, Verbosity enum
- **`generators.h`** - `IGenerator<T>`, `Generator<T>`, `SchemaBackedGenerator<T>`, `FunctionBackedGenerator<T>` with `map()`, `flatmap()`, `filter()` combinators
- **`strategies.h`** - Strategy factory functions in `hegel::strategies` namespace
- **`internal.h`** - `communicate_with_socket()`, `assume()`, `note()`, `stop_test()`
- **`src/protocol.h`** - Binary packet protocol, Connection, Channel classes
- **`src/socket.h`** - Low-level socket I/O

### Generator Pattern

Generators have two paths:
1. **Schema-based**: Generator has a CBOR schema value, sends single request to server (preferred for shrinking)
2. **Function-based**: Generator wraps a callable, may make multiple requests (used after `map()`/`filter()`)

```cpp
// Schema-based - single request, good shrinking
auto gen = integers<int>({.min_value = 0, .max_value = 100});

// Function-based - loses schema after transformation
auto squared = gen.map([](int x) { return x * x; });
```

## Code Style

Michael (mgibson) has carefully curated this codebase. Match his conventions exactly:

- **Formatting**: LLVM base style, 4-space indentation, left-aligned pointers (`int*`). Run `just format` before committing.
- **Headers**: Use `.h` extension (not `.hpp`)
- **Namespaces**: `hegel` for public API, `hegel::strategies` for strategies, `hegel::internal` for internals referenced in public headers, `hegel::impl::*` for purely private implementation
- **Includes**: Public headers use relative includes (`#include "options.h"`), source files use angle brackets for both public (`<hegel/internal.h>`) and private (`<socket.h>`) headers
- **File organization**: Each focused `.cpp` has a corresponding `.h` in `src/`. Private headers live in `src/`, not `include/`
- **Public API surface**: Minimal. Only what users need goes in `include/hegel/`. Internal details hidden via `@cond INTERNAL` / `@endcond` in Doxygen
- **Section dividers**: Use `// =====...=====` comment blocks
- **Parameter structs**: Designated initializers (C++20): `integers<int>({.min_value = 0})`
- **Self-contained**: Prefer small standalone implementations over adding heavy dependencies
- **No over-engineering**: Keep changes minimal and focused. Don't add abstractions beyond what's needed

### Error Handling: `assume()` vs exceptions

`hegel::internal::assume(condition)` is **only** for filtering generated test data that doesn't meet preconditions. It signals to the framework that the current test case should be silently discarded (via `HegelReject`), not counted as a failure. It must never be used to handle errors in the SDK implementation itself.

**Correct use** - filtering generated values in tests:
```cpp
auto x = integers<int>().generate();
assume(x != std::numeric_limits<int32_t>::min());  // Skip edge case
```

**Wrong use** - masking server/protocol errors:
```cpp
// BAD: silently swallows a server error as if it were bad test data
internal::assume(response.contains("result"));

// GOOD: surface the error so it can be diagnosed and fixed
if (!response.contains("result")) {
    throw std::runtime_error("Server response missing 'result' field");
}
```

Rules of thumb:
- Server returned an error or malformed response? Throw `std::runtime_error`.
- Caller passed invalid arguments (e.g. empty vector)? Throw `std::invalid_argument`.
- Generated test data doesn't meet a precondition? Use `assume()`.
