# Hegel C++ SDK

Hegel C++ SDK.

## Installation

To install, add the following to your `CMakeLists.txt`:

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

During build, `hegel-cpp`:

* Looks for `hegel` on PATH
* Otherwise, installs hegel with uv
   * Looks for `uv` on PATH
   * Otherwise, installs uv from installer

`hegel-cpp` build artifacts are stored in `CMAKE_BINARY_DIR/_deps/hegel`.

### Nix

To use hegel-cpp in Nix:

```nix
{
  description = "sandbox";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hegel-cpp.url = "git+ssh://git@github.com/antithesishq/hegel-cpp";
  };

  outputs = { self, nixpkgs, hegel-cpp, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      packages.${system}.default = hegel-cpp.lib.mkHegelCppProject {
        inherit pkgs;
        pname = "my-project";
        version = "0.1.0";
        src = ./.;
      };

      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ self.packages.${system}.default ];
      };
    };
}
```

## Development

Prerequisites:

- [CMake](https://cmake.org/) 3.14+
- [just](https://github.com/casey/just)
- [clang-format](https://clang.llvm.org/docs/ClangFormat.html)

Dependencies:
- C++20 compiler
- [reflect-cpp](https://github.com/getml/reflect-cpp) v0.22.0
- [nlohmann/json](https://github.com/nlohmann/json) v3.12.0

Commands:
```bash
just test
just format
```

## Quick Start

```cpp
#include "hegel/hegel.hpp"

int main() {
    hegel::hegel([]() {
        using namespace hegel::st;
        auto x = integers<int>().generate();
        auto y = integers<int>().generate();

        if (x + y != y + x) {
            throw std::runtime_error("commutativity violated");
        }
    });

    return 0;
}
```

## Configuration

Use `HegelOptions` for more control:

```cpp
#include "hegel/hegel.hpp"

int main() {
    hegel::hegel([]() {
        using namespace hegel::st;
        auto n = integers<int>({.min_value = 0, .max_value = 100}).generate();

        if (n < 0 || n > 100) {
            throw std::runtime_error("out of bounds");
        }
    }, {.test_cases = 500, .verbosity = hegel::Verbosity::Verbose});

    return 0;
}
```

## API Reference

### Type-Based Generation

Use `default_generator<T>()` to generate values based on the type's structure:

```cpp
struct Point {
    double x;
    double y;
};

hegel::hegel([]() {
    auto gen = hegel::default_generator<Point>();
    Point p = gen.generate();
});
```

### Strategies

All strategies are in the `hegel::st` namespace.

#### Primitives

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    // Null values
    auto n = nulls().generate();

    // Booleans
    auto b = booleans().generate();

    // Constant values
    auto c = just(42).generate();
    auto s = just("hello").generate();
});
```

#### Numbers

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    // Integers with optional bounds (uses type limits by default)
    auto i = integers<int>().generate();
    auto bounded = integers<int>({.min_value = 0, .max_value = 100}).generate();
    auto min_only = integers<int>({.min_value = 0}).generate();

    // Floating point
    auto f = floats<double>().generate();
    auto bounded_f = floats<double>({
        .min_value = 0.0,
        .max_value = 1.0,
        .exclude_min = true,  // exclusive bounds
        .exclude_max = true
    }).generate();
});
```

#### Strings

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    // Text with optional length constraints
    auto s = text().generate();
    auto bounded = text({.min_size = 1, .max_size = 100}).generate();

    // Regex patterns
    auto pattern = from_regex("[a-z]{3}-[0-9]{3}").generate();

    // Format strings
    auto email = emails().generate();
    auto url = urls().generate();
    auto domain = domains({.max_length = 63}).generate();
    auto ip = ip_addresses().generate();          // IPv4 or IPv6
    auto ipv4 = ip_addresses({.v = 4}).generate(); // IPv4 only
    auto ipv6 = ip_addresses({.v = 6}).generate(); // IPv6 only

    // Date/time (ISO 8601)
    auto date = dates().generate();      // YYYY-MM-DD
    auto time = times().generate();      // HH:MM:SS
    auto datetime = datetimes().generate();
});
```

#### Collections

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    // Vectors
    auto vec = vectors(integers<int>()).generate();
    auto bounded = vectors(integers<int>(), {
        .min_size = 1,
        .max_size = 10,
        .unique = true  // no duplicates
    }).generate();

    // Sets
    auto set = sets(integers<int>(), {.min_size = 1, .max_size = 5}).generate();

    // Dictionaries (string keys only, JSON limitation)
    auto dict = dictionaries(text(), integers<int>(), {
        .min_size = 1,
        .max_size = 3
    }).generate();
});
```

#### Tuples

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    auto pair = tuples(integers<int>(), text()).generate();
    auto triple = tuples(booleans(), integers<int>(), floats<double>()).generate();
});
```

#### Combinators

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    // Sample from fixed set
    auto color = sampled_from({"red", "green", "blue"}).generate();
    auto num = sampled_from({1, 2, 3, 4, 5}).generate();

    // Choose from multiple generators
    auto n = one_of({
        integers<int>({.min_value = 0, .max_value = 10}),
        integers<int>({.min_value = 100, .max_value = 110})
    }).generate();

    // Optional values
    auto opt = optional_(integers<int>()).generate();

    // Variants
    auto var = variant_(integers<int>(), text(), booleans()).generate();
});
```

### Combinators: map, flatmap, filter

```cpp
hegel::hegel([]() {
    using namespace hegel::st;

    // Transform values
    auto squared = integers<int>({.min_value = 1, .max_value = 10})
        .map([](int x) { return x * x; })
        .generate();

    // Filter values (rejects after 3 failures)
    auto even = integers<int>({.min_value = 0, .max_value = 100})
        .filter([](int x) { return x % 2 == 0; })
        .generate();

    // Dependent generation
    auto sized_string = integers<size_t>({.min_value = 1, .max_value = 10})
        .flatmap([](size_t len) {
            return text({.min_size = len, .max_size = len});
        })
        .generate();
});
```

### Object Construction

```cpp
struct Rectangle {
    int width;
    int height;
};

hegel::hegel([]() {
    using namespace hegel::st;

    // Positional construction (calls constructor)
    auto rect = builds<Rectangle>(
        integers<int>({.min_value = 1, .max_value = 100}),
        integers<int>({.min_value = 1, .max_value = 100})
    ).generate();

    // Named aggregate initialization
    auto rect2 = builds_agg<Rectangle>(
        field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value = 100})),
        field<&Rectangle::height>(integers<int>({.min_value = 1, .max_value = 100}))
    ).generate();
});
```

### Assumptions

Use `assume()` when generated data doesn't meet preconditions:

```cpp
hegel::hegel([]() {
    auto person = person_gen.generate();
    hegel::assume(person.age >= 18);

    // Test logic for adults only...
});
```

This tells Hegel to try different input data rather than counting as a test failure.
