> [!IMPORTANT]
> We're excited you're checking out Hegel! Hegel is in beta, and we'd love for you to try it and [report any feedback](https://github.com/hegeldev/hegel-cpp/issues/new).
>
> As part of our beta, we may make breaking changes if it makes Hegel a better property-based testing library. If that instability bothers you, please check back in a few months for a stable release!
>
> See https://hegel.dev/compatibility for more details.

# Hegel for C++

* [Documentation](https://hegel.dev/cpp)
* [Website](https://hegel.dev)

Hegel is a property-based testing library for C++. Hegel is based on [Hypothesis](https://github.com/hypothesisworks/hypothesis), using the [Hegel protocol](https://hegel.dev/).

## Installation

Hegel requires C++20.

To install with CMake, add this to your `CMakeLists.txt` (CMake 3.14+ required):

```cmake
include(FetchContent)
FetchContent_Declare(
    hegel
    GIT_REPOSITORY https://github.com/hegeldev/hegel-cpp.git
    GIT_TAG v0.3.4
)
FetchContent_MakeAvailable(hegel)

target_link_libraries(your_target PRIVATE hegel)
```

Hegel will use uv to install the required [hegel-core](https://github.com/hegeldev/hegel-core) server component.
If `uv` is already on your path, it will use that, otherwise it will download a private copy of it to ~/.cache/hegel and not put it on your path. See https://hegel.dev/reference/installation for details.

## Quickstart

Here's a quick example of how to write a Hegel test:

```cpp
#include <hegel/hegel.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace gs = hegel::generators;

std::vector<int> my_sort(std::vector<int> ls) {
    std::sort(ls.begin(), ls.end());
    ls.erase(std::unique(ls.begin(), ls.end()), ls.end());
    return ls;
}

int main() {
    hegel::test([](hegel::TestCase& tc) {
        auto vec1 = tc.draw(gs::vectors(gs::integers<int>()));
        auto vec2 = my_sort(vec1);
        std::sort(vec1.begin(), vec1.end());
        if (vec1 != vec2) {
            throw std::runtime_error("sort mismatch");
        }
    });

    return 0;
}
```

This test will fail! Hegel will produce a minimal failing test case for us:

```
Generated: [0,0]
libc++abi: terminating due to uncaught exception of type std::runtime_error:
Hegel test failed: sort mismatch
```

Hegel reports the minimal example showing that our sort is incorrectly dropping duplicates.
