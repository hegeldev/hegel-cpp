# Hegel for C++

> [!IMPORTANT]
> We're excited you're checking out Hegel! Hegel in general is in beta, and we'd love for you to try it and [report any feedback](https://github.com/hegeldev/hegel-cpp/issues/new).
>
> The C++ library in particular is not currently one of our "blessed" implementations. It's far more rough around the edges than the more mature libraries. We eventually intend to support it more thoroughly, but right now it falls short of our standards for a high quality Hegel library, and will often lag behind the more complete implementations.
>
> We're using it in practice ourselves, so we're confident it works well enough to be useful, but it's currently less user friendly than we'd like and you are more likely to run into issues. You are more than welcome to use it anyway, and please do report any problems you run into while doing so.

Hegel is a property-based testing library for C++. Hegel is based on [Hypothesis](https://github.com/hypothesisworks/hypothesis), using the [Hegel protocol](https://hegel.dev/).

## Installation

Add this to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    hegel
    GIT_REPOSITORY https://github.com/hegeldev/hegel-cpp.git
    GIT_TAG v0.2.9
)
FetchContent_MakeAvailable(hegel)

target_link_libraries(your_target PRIVATE hegel)
```

Hegel will use [uv](https://docs.astral.sh/uv/) to install the required [hegel-core](https://github.com/hegeldev/hegel-core) server component.
If `uv` is already on your path, it will use that, otherwise it will download a private copy of it to ~/.cache/hegel and not put it on your path.
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

This test will fail! Hegel will produce a minimal failing test case for us, reporting the minimal example showing that our sort is incorrectly dropping duplicates (the input `[0, 0]` sorts to `[0, 0]` but `my_sort` returns `[0]`). If we remove the `std::unique` call from `my_sort()`, this test will then pass (because it's just comparing the standard sort against itself).
