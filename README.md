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
    GIT_TAG v0.7.1
)
FetchContent_MakeAvailable(hegel)

target_link_libraries(your_target PRIVATE hegel)
```

At configure time the build downloads a small prebuilt shared library
([libhegel](https://github.com/hegeldev/hegel-rust/tree/main/hegel-c),
Hegel's native engine) for your platform and verifies it against its published
SHA-256, then links it. To link a locally built engine instead, pass 
`-DHEGEL_LIBHEGEL_LIBRARY=/path/to/libhegel_c.<ext>`. 
See https://hegel.dev/reference/installation for details.

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

HEGEL_TEST(sort_agrees_with_std_sort)(hegel::TestCase& tc) {
    auto vec1 = tc.draw(gs::vectors(gs::integers<int>()));
    auto vec2 = my_sort(vec1);
    std::sort(vec1.begin(), vec1.end());
    if (vec1 != vec2) {
        throw std::runtime_error("sort mismatch");
    }
}

int main() {
    sort_agrees_with_std_sort();
    return 0;
}
```

`HEGEL_TEST` defines the test as a plain function you can invoke from `main()`
or from any test framework, and names the test in Hegel's example database so
failures found in one run are replayed first in the next. (You can also call
`hegel::test(callback, settings)` directly.)

This test will fail! Hegel will produce a minimal failing test case for us:

```
Generated: [0,0]
libc++abi: terminating due to uncaught exception of type std::runtime_error:
Hegel test failed: sort mismatch
```

Hegel reports the minimal example showing that our sort is incorrectly dropping duplicates.
