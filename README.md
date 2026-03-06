# hegel-cpp

A C++ SDK for [Hegel](https://github.com/antithesishq/hegel-core) — universal
property-based testing powered by [Hypothesis](https://hypothesis.works/).

Hegel generates random inputs for your tests, finds failures, and automatically
shrinks them to minimal counterexamples.

## Installation

Add to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    hegel
    GIT_REPOSITORY https://github.com/antithesishq/hegel-cpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(hegel)

target_link_libraries(your_target PRIVATE hegel)
```

The `hegel` CLI is installed automatically during the CMake build. If you
already have it on your PATH, it will be used instead.

## Quick Start

```cpp
#include <hegel/hegel.h>
#include <stdexcept>

using namespace hegel::generators;

int main() {
    hegel::hegel([]() {
        auto x = hegel::draw(integers<int>());
        auto y = hegel::draw(integers<int>());

        // Addition should be commutative
        if (x + y != y + x) {
            throw std::runtime_error("commutativity violated");
        }
    });

    return 0;
}
```

Hegel generates 100 random input pairs and reports the minimal counterexample
if it finds one.

For a full walkthrough, see [docs/getting-started.md](docs/getting-started.md).

## Development

Prerequisites: C++20 compiler, CMake 3.14+, [just](https://github.com/casey/just),
[clang-format](https://clang.llvm.org/docs/ClangFormat.html).

```bash
just check   # Full CI: build + test + format check
just test    # Build and run tests
just format  # Auto-format code
just docs    # Build Doxygen documentation
```
