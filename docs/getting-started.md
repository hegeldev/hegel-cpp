# Getting Started with Hegel for C++

Hegel is a universal property-based testing protocol. Instead of writing
individual test cases by hand, you describe the *properties* your code should
satisfy and let Hegel generate random inputs to find counterexamples. When a
failing input is found, Hegel automatically shrinks it to a minimal
reproducing case.

This guide walks you through Hegel for C++ from first test to advanced
generation techniques.

## Install Hegel

Add hegel-cpp to your project with CMake FetchContent:

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

During the build, hegel-cpp will automatically install the `hegel` server via `uv` if it is not already on your
PATH.

Requirements: a C++20 compiler and CMake 3.14 or later.

## Write your first test

Create a file called `test_add.cpp`:

```cpp
#include <hegel/hegel.h>
#include <stdexcept>

namespace gs = hegel::generators;

int main() {
    hegel::hegel([]() {
        auto x = hegel::draw(gs::integers<int>());
        auto y = hegel::draw(gs::integers<int>());

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
}, {.test_cases = 500, .verbosity = hegel::settings::Verbosity::Verbose});
```

## Generating multiple values

Use `hegel::draw()` to draw one value from a generator. Call it
multiple times to get independent values:

```cpp
namespace gs = hegel::generators;

hegel::hegel([]() {
    auto a = hegel::draw(gs::integers<int>({.min_value = 1, .max_value = 100}));
    auto b = hegel::draw(gs::integers<int>({.min_value = 1, .max_value = 100}));
    auto s = hegel::draw(gs::text({.min_size = 0, .max_size = 50}));

    // Use a, b, and s in your property check ...
});
```

You can also store a generator and reuse it:

```cpp
namespace gs = hegel::generators;

hegel::hegel([]() {
    auto small_int = gs::integers<int>({.min_value = -10, .max_value = 10});

    auto x = hegel::draw(small_int);
    auto y = hegel::draw(small_int);
    auto z = hegel::draw(small_int);

    // (x + y) + z == x + (y + z)
    if ((x + y) + z != x + (y + z)) {
        throw std::runtime_error("associativity violated");
    }
});
```

## Filtering

Use `.filter()` to reject values that do not satisfy a predicate:

```cpp
namespace gs = hegel::generators;

hegel::hegel([]() {
    auto nonzero = gs::integers<int>({.min_value = -100, .max_value = 100})
        .filter([](int x) { return x != 0; });

    auto dividend = hegel::draw(gs::integers<int>());
    auto divisor  = hegel::draw(nonzero);

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
namespace gs = hegel::generators;

hegel::hegel([]() {
    // Generate even numbers by doubling
    auto even = gs::integers<int>({.min_value = 0, .max_value = 50})
        .map([](int x) { return x * 2; });

    auto n = hegel::draw(even);

    if (n % 2 != 0) {
        throw std::runtime_error("expected even number");
    }
});
```

You can chain `.map()` calls:

```cpp
auto upper_letter = gs::integers<int>({.min_value = 0, .max_value = 25})
    .map([](int i) { return static_cast<char>('A' + i); })
    .map([](char c) { return std::string(1, c); });
```

## Dependent generation

When the parameters of one generator depend on the output of another, use
`.flat_map()`. The function you pass receives a generated value and returns
a new generator:

```cpp
namespace gs = hegel::generators;

hegel::hegel([]() {
    // Generate a length, then a string of exactly that length
    auto sized_string = gs::integers<size_t>({.min_value = 1, .max_value = 20})
        .flat_map([](size_t len) {
            return gs::text({.min_size = len, .max_size = len});
        });

    auto s = hegel::draw(sized_string);

    if (s.empty()) {
        throw std::runtime_error("expected non-empty string");
    }
});
```

A more realistic example -- generating a vector and then picking an index
into it:

```cpp
namespace gs = hegel::generators;

hegel::hegel([]() {
    auto vec = hegel::draw(gs::vectors(gs::integers<int>(), {.min_size = 1, .max_size = 20}));

    auto idx = hegel::draw(gs::integers<size_t>({
        .min_value = 0,
        .max_value = static_cast<int64_t>(vec.size() - 1)
    }));

    // idx is always a valid index into vec
    auto element = vec.at(idx);
});
```

## What you can generate

All generator factory functions live in the `hegel::generators` namespace.
Alias it with `namespace gs = hegel::generators;` for brevity.

### Primitive types

```cpp
auto b = hegel::draw(gs::booleans());                          // bool
auto i = hegel::draw(gs::integers<int>());                     // int (any integer type)
auto n = hegel::draw(gs::integers<int64_t>({.min_value = 0})); // bounded int64_t
auto f = hegel::draw(gs::floats<double>());                    // double
auto g = hegel::draw(gs::floats<float>({                       // float with options
    .min_value = 0.0f,
    .max_value = 1.0f,
    .allow_nan = false,
    .allow_infinity = false
}));
auto s = hegel::draw(gs::text());                              // std::string
auto t = hegel::draw(gs::text({.min_size = 5, .max_size = 10}));
auto raw = hegel::draw(gs::binary({.min_size = 16, .max_size = 16})); // vector<uint8_t>
```

### Constants and choices

```cpp
auto always_42 = hegel::draw(gs::just(42));
auto color = hegel::draw(gs::sampled_from({"red", "green", "blue"}));
auto die = hegel::draw(gs::sampled_from({1, 2, 3, 4, 5, 6}));
```

### Collections

```cpp
auto vec = hegel::draw(gs::vectors(gs::integers<int>(), {.min_size = 1, .max_size = 10}));
auto unique_vec = hegel::draw(gs::vectors(gs::integers<int>(), {.unique = true}));
auto s = hegel::draw(gs::sets(gs::text(), {.min_size = 1, .max_size = 5}));
auto m = hegel::draw(gs::dictionaries(gs::text(), gs::integers<int>(), {.max_size = 3}));
auto pair = hegel::draw(gs::tuples(gs::integers<int>(), gs::text()));
```

### Random

```cpp
auto rng = hegel::draw(gs::randoms());
std::uniform_real_distribution<double> dist(0.0, 10.0);
double uniform_value = dist(rng);

// Using true random
auto rng2 = hegel::draw(gs::randoms({ .use_true_random = true }));
std::lognormal_distribution<double> dist2(0.0, 10.0);
double lognormal_value = dist2(rng2);
```

### Combinators

```cpp
// Choose from several generators
auto n = hegel::draw(gs::one_of({
    gs::integers<int>({.min_value = 0, .max_value = 10}),
    gs::integers<int>({.min_value = 90, .max_value = 100})
}));

// Optional values (some or none)
auto opt = hegel::draw(gs::optional_(gs::text())); // std::optional<std::string>

// Variant types
auto var = hegel::draw(gs::variant_(gs::integers<int>(), gs::text(), gs::booleans()));
```

### Formats and addresses

```cpp
auto email = hegel::draw(gs::emails());                        // e.g. "user@example.com"
auto url   = hegel::draw(gs::urls());                          // e.g. "https://example.com/path"
auto dom   = hegel::draw(gs::domains());                       // e.g. "sub.example.org"
auto ipv4  = hegel::draw(gs::ip_addresses({.v = 4}));          // e.g. "192.168.1.1"
auto ipv6  = hegel::draw(gs::ip_addresses({.v = 6}));          // e.g. "::1"
auto d     = hegel::draw(gs::dates());                         // ISO 8601: "2024-03-15"
auto t     = hegel::draw(gs::times());                         // "14:30:00"
auto dt    = hegel::draw(gs::datetimes());                     // "2024-03-15T14:30:00"
auto pat   = hegel::draw(gs::from_regex("[A-Z]{2}-[0-9]{4}")); // e.g. "QX-8271"
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
        auto p = hegel::draw(gs::default_generator<Point>());

        // p.x and p.y are random doubles
    });

    return 0;
}
```

For more control over how each field is generated, use `builds` for
positional construction or `builds_agg` for named-field construction:

```cpp
namespace gs = hegel::generators;

struct Rectangle {
    int width;
    int height;
};

hegel::hegel([]() {
    // Positional: calls Rectangle{width, height}
    auto rect = hegel::draw(gs::builds<Rectangle>(
        gs::integers<int>({.min_value = 1, .max_value = 100}),
        gs::integers<int>({.min_value = 1, .max_value = 100})
    ));

    // Named fields: explicit binding to struct members
    auto rect2 = hegel::draw(gs::builds_agg<Rectangle>(
        gs::field<&Rectangle::width>(gs::integers<int>({.min_value = 1, .max_value = 50})),
        gs::field<&Rectangle::height>(gs::integers<int>({.min_value = 1, .max_value = 50}))
    ));

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

namespace gs = hegel::generators;

int main() {
    hegel::hegel([]() {
        auto ops = hegel::draw(gs::vectors(
            gs::sampled_from({"push", "pop", "peek"}),
            {.min_size = 1, .max_size = 20}
        ));

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
    namespace gs = hegel::generators;
    auto a = hegel::draw(gs::integers<int>());
    auto b = hegel::draw(gs::integers<int>());

    // Only test when no overflow occurs
    hegel::assume(
        b == 0 || (a / b) * b + (a % b) == a
    );
});
```

## Guiding generation with target()

> **Note:** `target()` is not yet available in Hegel for C++. This section
> describes the concept for reference; it will be added in a future release.

In other Hegel libraries, `target(value, label)` lets you steer generation
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
- See the [hegel-core](https://github.com/hegeldev/hegel-core)
  repository for details on the Hegel server and the binary protocol.
