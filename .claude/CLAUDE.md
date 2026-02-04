# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the C++ SDK for Hegel, a universal property-based testing framework. The SDK communicates with a Python server (powered by Hypothesis) via Unix sockets to generate random test data and perform shrinking.

## Build & Test Commands

```bash
just test     # cmake -B build && cmake --build build && ctest
just format   # clang-format on all .cpp/.hpp files
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
- nlohmann/json v3.12.0 (JSON manipulation)
- Google Test (for unit tests)

## Architecture

### Execution Modes

The SDK supports two modes:

1. **Embedded Mode** (recommended): Test binary spawns `hegel` as a subprocess via `hegel::hegel()`. Creates a Unix socket server, forks the Hegel CLI, and handles test execution/shrinking internally.

2. **External Mode**: Hegel CLI runs the test binary as subprocess. The binary reads `HEGEL_SOCKET` environment variable and communicates with an existing server.

### Key Components

- **`hegel.h`** - Main include file, aggregates all SDK components
- **`core.h`** - Mode enum, HegelOptions, `assume()`, `note()`, HegelReject exception
- **`embedded.h`** - `hegel()` function template that orchestrates embedded mode
- **`generators.h`** - `Generator<T>` and `DefaultGenerator<T>` class templates with `map()`, `flatmap()`, `filter()` combinators
- **`strategies.h`** - Strategy factory functions in `hegel::strategies` namespace (integers, floats, text, vectors, etc.)
- **`detail.h`** - Internal socket communication, JSON protocol handling
- **`grouping.h`** - Span management for shrinking

### Generator Pattern

Generators have two paths:
1. **Schema-based**: Generator has a JSON schema string, sends single request to server (preferred for shrinking)
2. **Function-based**: Generator wraps a callable, may make multiple requests (used after `map()`/`filter()`)

```cpp
// Schema-based - single request, good shrinking
auto gen = integers<int>({.min_value = 0, .max_value = 100});

// Function-based - loses schema after transformation
auto squared = gen.map([](int x) { return x * x; });
```

### Protocol

Newline-delimited JSON over Unix socket. The SDK sends JSON schemas and receives generated values. Key environment variables:
- `HEGEL_SOCKET` - Unix socket path (set by Hegel CLI or embedded mode)
- `HEGEL_REJECT_CODE` - Exit code for `assume(false)` in external mode

## Code Style

- All public API is in `hegel` namespace, strategies in `hegel::strategies`
- Parameter structs use designated initializers (C++20): `integers<int>({.min_value = 0})`
- Dictionary keys must be strings (JSON schema limitation)
