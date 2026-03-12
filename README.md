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

### Hegel server

The SDK automatically manages the `hegel` server binary. During the CMake
configure step it creates a project-local `.hegel/venv` virtualenv and installs
the pinned version of [hegel-core](https://github.com/antithesishq/hegel-core)
into it. Subsequent configures reuse the cached binary unless the pinned
version changes.

To use your own `hegel` binary instead (e.g. a local development build), set
the `HEGEL_SERVER_COMMAND` environment variable:

```bash
export HEGEL_SERVER_COMMAND=/path/to/hegel
```

The SDK requires [`uv`](https://docs.astral.sh/uv/) on PATH for automatic
server management.

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

Prerequisites: C++20 compiler, CMake 3.14+, [uv](https://docs.astral.sh/uv/),
[just](https://github.com/casey/just),
[clang-format](https://clang.llvm.org/docs/ClangFormat.html).

```bash
just check   # Full CI: build + test + format check
just test    # Build and run tests
just format  # Auto-format code
just docs    # Build Doxygen documentation
```
