# Getting Started with Hegel for C++

Hegel is a universal property-based testing framework. Instead of writing
individual test cases by hand, you describe the *properties* your code should
satisfy and let Hegel generate random inputs to find counterexamples. When a
failing input is found, Hegel automatically shrinks it to a minimal
reproducing case.

This guide walks you through the C++ SDK from first test to advanced
generation techniques.

## Install Hegel

Add hegel-cpp to your project with CMake FetchContent:

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

During the build, hegel-cpp will automatically install the `hegel` CLI
(which provides the `hegeld` server) via `uv` if it is not already on your
PATH.

Requirements: a C++20 compiler and CMake 3.14 or later.

## Write your first test

Create a file called `test_add.cpp`:

```cpp
#include <hegel/hegel.h>
#include <stdexcept>

using namespace hegel::generators;

int main() {
    hegel::hegel([]() {
        auto x = integers<int>().generate();
        auto y = integers<int>().generate();

        // Addition should be commutative
        if (x + y != y + x) {
            throw std::runtime_error("commutativity violated");
        }
    });

    return 0;
}
```

`hegel::hegel()` runs the lambda 100 times, each time with fresh random
values. If the lambda throws, Hegel records the failure and then replays
the test with progressively simpler inputs until it finds a minimal
counterexample.

## Running in a test suite

Each Hegel test is a standalone executable. You can integrate it with CTest
in the usual way:

```cmake
add_executable(test_add test_add.cpp)
target_link_libraries(test_add PRIVATE hegel)
add_test(NAME test_add COMMAND test_add)
```

To run more or fewer test cases, or to enable verbose output:

```cpp
hegel::hegel([]() {
    // ...
}, {.test_cases = 500, .verbosity = hegel::Verbosity::Verbose});
```

## Generating multiple values

Every generator has a `.generate()` method that draws one value. Call it
multiple times to get independent values:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    auto a = integers<int>({.min_value = 1, .max_value = 100}).generate();
    auto b = integers<int>({.min_value = 1, .max_value = 100}).generate();
    auto s = text({.min_size = 0, .max_size = 50}).generate();

    // Use a, b, and s in your property check ...
});
```

You can also store a generator and reuse it:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    auto small_int = integers<int>({.min_value = -10, .max_value = 10});

    auto x = small_int.generate();
    auto y = small_int.generate();
    auto z = small_int.generate();

    // (x + y) + z == x + (y + z)
    if ((x + y) + z != x + (y + z)) {
        throw std::runtime_error("associativity violated");
    }
});
```

## Filtering

Use `.filter()` to reject values that do not satisfy a predicate:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    auto nonzero = integers<int>({.min_value = -100, .max_value = 100})
        .filter([](int x) { return x != 0; });

    auto dividend = integers<int>().generate();
    auto divisor  = nonzero.generate();

    // Safe to divide -- divisor is guaranteed non-zero
    auto quotient  = dividend / divisor;
    auto remainder = dividend % divisor;

    if (quotient * divisor + remainder != dividend) {
        throw std::runtime_error("division identity violated");
    }
});
```

Filtering works best when the predicate accepts most values. Hegel retries
up to 3 times per draw; if the predicate keeps failing, use `assume()`
inside the test body instead.

## Transforming generated values

Use `.map()` to transform generated values without changing the underlying
generation or shrinking strategy:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    // Generate even numbers by doubling
    auto even = integers<int>({.min_value = 0, .max_value = 50})
        .map([](int x) { return x * 2; });

    auto n = even.generate();

    if (n % 2 != 0) {
        throw std::runtime_error("expected even number");
    }
});
```

You can chain `.map()` calls:

```cpp
auto upper_letter = integers<int>({.min_value = 0, .max_value = 25})
    .map([](int i) { return static_cast<char>('A' + i); })
    .map([](char c) { return std::string(1, c); });
```

## Dependent generation

When the parameters of one generator depend on the output of another, use
`.flat_map()`. The function you pass receives a generated value and returns
a new generator:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    // Generate a length, then a string of exactly that length
    auto sized_string = integers<size_t>({.min_value = 1, .max_value = 20})
        .flat_map([](size_t len) {
            return text({.min_size = len, .max_size = len});
        });

    auto s = sized_string.generate();

    if (s.empty()) {
        throw std::runtime_error("expected non-empty string");
    }
});
```

A more realistic example -- generating a vector and then picking an index
into it:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    auto vec = vectors(integers<int>(), {.min_size = 1, .max_size = 20}).generate();

    auto idx = integers<size_t>({
        .min_value = 0,
        .max_value = static_cast<int64_t>(vec.size() - 1)
    }).generate();

    // idx is always a valid index into vec
    auto element = vec.at(idx);
});
```

## What you can generate

All generator factory functions live in the `hegel::generators` namespace.
Import them with `using namespace hegel::generators;`.

### Primitive types

```cpp
auto b = booleans().generate();                          // bool
auto i = integers<int>().generate();                     // int (any integer type)
auto n = integers<int64_t>({.min_value = 0}).generate(); // bounded int64_t
auto f = floats<double>().generate();                    // double
auto g = floats<float>({                                 // float with options
    .min_value = 0.0f,
    .max_value = 1.0f,
    .allow_nan = false,
    .allow_infinity = false
}).generate();
auto s = text().generate();                              // std::string
auto t = text({.min_size = 5, .max_size = 10}).generate();
auto raw = binary({.min_size = 16, .max_size = 16}).generate(); // vector<uint8_t>
```

### Constants and choices

```cpp
auto always_42 = just(42).generate();
auto color = sampled_from({"red", "green", "blue"}).generate();
auto die = sampled_from({1, 2, 3, 4, 5, 6}).generate();
```

### Collections

```cpp
auto vec = vectors(integers<int>(), {.min_size = 1, .max_size = 10}).generate();
auto unique_vec = vectors(integers<int>(), {.unique = true}).generate();
auto s = sets(text(), {.min_size = 1, .max_size = 5}).generate();
auto m = dictionaries(text(), integers<int>(), {.max_size = 3}).generate();
auto pair = tuples(integers<int>(), text()).generate();
```

### Random

Two modes:
- **Artificial (default)**: Each `operator()` call sends a generate
  request to hegeld, enabling per-value shrinking.
  Artificial mode may cause the following Hypothesis health check failure:

  "The smallest natural input for this test is very large. 
  This makes it difficult for Hypothesis to generate good inputs, 
  especially when trying to shrink failing inputs"

  when used with distributions implemented via rejection sampling, such as
  std::normal_distribution. Alternatively, you may set use_true_random = true
  to continue using distributions from the standard library, but if you require
  artificial randomness, you must use the Hegel version of the distribution.
- **True random**: A single seed is generated via Hypothesis at
  construction time, then all calls use a local `std::mt19937`.
  Faster but only the seed shrinks.

```cpp
auto rng = randoms().generate();
// Use with non-rejection sampling based <random> distribution
std::uniform_real_distribution<double> dist(0.0, 10.0);
double uniform_value = dist(rng);
// Using true random
auto rng = randoms({ .use_true_random = true }).generate();
std::lognormal_distribution<double> dist(0.0, 10.0);
double uniform_value = dist(rng);
```

### Combinators

```cpp
// Choose from several generators
auto n = one_of({
    integers<int>({.min_value = 0, .max_value = 10}),
    integers<int>({.min_value = 90, .max_value = 100})
}).generate();

// Optional values (some or none)
auto opt = optional_(text()).generate(); // std::optional<std::string>

// Variant types
auto var = variant_(integers<int>(), text(), booleans()).generate();
```

### Formats and addresses

```cpp
auto email = emails().generate();                        // e.g. "user@example.com"
auto url   = urls().generate();                          // e.g. "https://example.com/path"
auto dom   = domains().generate();                       // e.g. "sub.example.org"
auto ipv4  = ip_addresses({.v = 4}).generate();          // e.g. "192.168.1.1"
auto ipv6  = ip_addresses({.v = 6}).generate();          // e.g. "::1"
auto d     = dates().generate();                         // ISO 8601: "2024-03-15"
auto t     = times().generate();                         // "14:30:00"
auto dt    = datetimes().generate();                     // "2024-03-15T14:30:00"
auto pat   = from_regex("[A-Z]{2}-[0-9]{4}").generate(); // e.g. "QX-8271"
```

## Type-directed derivation

For struct types, Hegel can derive a generator automatically using
reflect-cpp. Define your struct and call `default_generator<T>()`:

```cpp
#include <hegel/hegel.h>

struct Point {
    double x;
    double y;
};

int main() {
    hegel::hegel([]() {
        auto p = hegel::generators::default_generator<Point>().generate();

        // p.x and p.y are random doubles
    });

    return 0;
}
```

For more control over how each field is generated, use `builds` for
positional construction or `builds_agg` for named-field construction:

```cpp
using namespace hegel::generators;

struct Rectangle {
    int width;
    int height;
};

hegel::hegel([]() {
    // Positional: calls Rectangle{width, height}
    auto rect = builds<Rectangle>(
        integers<int>({.min_value = 1, .max_value = 100}),
        integers<int>({.min_value = 1, .max_value = 100})
    ).generate();

    // Named fields: explicit binding to struct members
    auto rect2 = builds_agg<Rectangle>(
        field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value = 50})),
        field<&Rectangle::height>(integers<int>({.min_value = 1, .max_value = 50}))
    ).generate();

    if (rect.width < 1 || rect.height < 1) {
        throw std::runtime_error("expected positive dimensions");
    }
});
```

## Debugging with note()

When a test fails, Hegel replays the failing input to shrink it. During
this final replay, messages logged with `hegel::note()` are
printed to stderr. This is useful for understanding the intermediate state
that led to a failure:

```cpp
#include <hegel/hegel.h>

using namespace hegel::generators;

int main() {
    hegel::hegel([]() {
        auto ops = vectors(
            sampled_from({"push", "pop", "peek"}),
            {.min_size = 1, .max_size = 20}
        ).generate();

        for (const auto& op : ops) {
            hegel::note("executing operation: " + std::string(op));
            // ... apply op to your data structure ...
        }
    });

    return 0;
}
```

Notes are only printed during the shrunk replay, not during the initial
exploration phase. This keeps output clean while still providing full
visibility into the minimal failing case.

Use `hegel::assume()` to skip test cases whose generated inputs
do not meet a precondition:

```cpp
hegel::hegel([]() {
    using namespace hegel::generators;
    auto a = integers<int>().generate();
    auto b = integers<int>().generate();

    // Only test when no overflow occurs
    hegel::assume(
        b == 0 || (a / b) * b + (a % b) == a
    );
});
```

## Guiding generation with target()

> **Note:** `target()` is not yet available in the C++ SDK. This section
> describes the concept for reference; it will be added in a future release.

In other Hegel SDKs, `target(value, label)` lets you steer generation
toward inputs that maximize a numeric metric (for example, code coverage
depth or collection size). When this feature becomes available in C++, it
will enable more effective exploration of edge cases that are hard to reach
by random sampling alone.

## Next steps

- Browse the [examples/](../examples/) directory for complete, runnable
  property-based tests covering data structures like bounded queues, LRU
  caches, and run-length encoding.
- Read the generated Doxygen documentation (`just docs`) for the full API
  reference.
- See the [hegel-core](https://github.com/antithesishq/hegel-core)
  repository for details on the Hegel server, the binary protocol, and the
  reference Python SDK.
