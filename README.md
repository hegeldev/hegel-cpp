# Hegel for C++

> [!IMPORTANT]
> We're excited you're checking out Hegel! Hegel in general is in beta, and we'd love for you to try it and [report any feedback](https://github.com/hegeldev/hegel-cpp/issues/new).
>
> The C++ library in particular is not currently one of our "blessed" implementations. It's far more rough around the edges than the more mature libraries. We eventually intend to support it more thoroughly, but right now it falls short of our standards for a high quality Hegel library, and will often lag behind the more complete implementations.
>
> We're using it in practice ourselves, so we're confident it works well enough to be useful, but it's currently less user friendly than we'd like and you are more likely to run into issues. You are more than welcome to use it anyway, and please do report any problems you run into while doing so. 

A C++ library for [Hegel](https://github.com/hegeldev/hegel-core) — universal
property-based testing powered by [Hypothesis](https://hypothesis.works/).

Hegel generates random inputs for your tests, finds failures, and automatically
shrinks them to minimal counterexamples.

## Installation

Add to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    hegel
    GIT_REPOSITORY https://github.com/hegeldev/hegel-cpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(hegel)

target_link_libraries(your_target PRIVATE hegel)
```

The Hegel server itself is a Python program. At CMake configure time, it is
installed into the build directory using [uv](https://github.com/astral-sh/uv)
(which is also auto-installed if missing), and the resulting path is baked into
the library. You do not need to put anything on your `PATH` — but if a `hegel`
binary is already on `PATH`, it will be used in preference to a fresh install.

Requirements: a C++20 compiler and CMake 3.14+. A working network connection is
needed on the first build so that `uv` can fetch `hegel` and its Python
dependencies.

## Quick start

```cpp
#include <hegel/hegel.h>
#include <stdexcept>

namespace gs = hegel::generators;

int main() {
    hegel::hegel([]() {
        auto x = hegel::draw(gs::integers<int>());
        auto y = hegel::draw(gs::integers<int>());

        // Addition should be commutative.
        if (x + y != y + x) {
            throw std::runtime_error("commutativity violated");
        }
    });

    return 0;
}
```

Each call to `hegel::hegel()` runs its test function 100 times by default
against fresh generated inputs. If the function throws, Hegel shrinks the
inputs to a minimal failing case and then throws `std::runtime_error` itself,
so the enclosing process exits non-zero. Plug that into CTest (or any other
runner) in the usual way:

```cmake
add_executable(test_add test_add.cpp)
target_link_libraries(test_add PRIVATE hegel)
add_test(NAME test_add COMMAND test_add)
```

To change the number of test cases, seed, or verbosity, pass a `HegelSettings`:

```cpp
hegel::hegel([]() {
    // ...
}, {.test_cases = 500, .verbosity = hegel::settings::Verbosity::Verbose});
```

For a full walkthrough, including generators, combinators, and type-directed
derivation via reflect-cpp, see
[docs/getting-started.md](docs/getting-started.md).

## Development

Prerequisites: C++20 compiler, CMake 3.14+, [just](https://github.com/casey/just),
[clang-format](https://clang.llvm.org/docs/ClangFormat.html).

```bash
just check   # Full CI: build + test + format check
just test    # Build and run tests
just format  # Auto-format code
just docs    # Build Doxygen documentation
```
