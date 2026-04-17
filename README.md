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

Here's a short example of a Hegel test:

```cpp
#include <hegel/hegel.h>
#include <stdexcept>

namespace gs = hegel::generators;

std::vector<int> my_sort(std::vector<int> v) {
    std::vector<int> v_(v);
    std::sort(v_.begin(), v_.end());
    auto last = std::unique(v_.begin(), v_.end());
    v_.erase(last, v_.end());
    return v_;
}

int main() {
    hegel::hegel([](hegel::TestCase& tc) {
        auto vec1 = tc.draw(gs::vectors(gs::integers<int>()));
        auto vec2 = my_sort(vec1);
        std::sort(vec1.begin(), vec1.end());

        if (vec1 != vec2) {
            throw std::runtime_error("vectors not equal");
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
add_executable(test_sort test_sort.cpp)
target_link_libraries(test_sort PRIVATE hegel)
add_test(NAME test_sort COMMAND test_sort)
```

This test will fail. Hegel will produce a minimal failing test case for us:
```
Generated: [0, 0]
libc++abi: terminating due to uncaught exception of type std::runtime_error: 
Hegel test failed: vectors not equal
```

Hegel reports the minimal example showing that our sort is incorrectly dropping duplicates. 
If we remove    
```cpp 
auto last = std::unique(v_.begin(), v_.end());
v_.erase(last, v_.end());
```
from `my_sort`, this test will then pass (because it's just comparing the standard sort against itself).

To change the number of test cases, seed, or verbosity, pass a `HegelSettings`:

```cpp
hegel::hegel([](hegel::TestCase& tc) {
    // ...
}, {.test_cases = 500, .verbosity = hegel::settings::Verbosity::Verbose, .seed = 1234});
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
