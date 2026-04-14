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
    GIT_REPOSITORY https://github.com/antithesishq/hegel-cpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(hegel)

target_link_libraries(your_target PRIVATE hegel)
```

During the build, hegel-cpp will automatically install the `hegel` server via `uv` if it is not already on your
PATH.

Requirements: a C++20 compiler and CMake 3.14 or later.

## Write your first test

Create a file called `test_sort.cpp`:

```cpp
#include <hegel/hegel.h>
#include <stdexcept>

using namespace hegel::generators;

std::vector<int> my_sort(std::vector<int> v) {
    std::vector<int> v_(v);
    std::sort(v_.begin(), v_.end());
    auto last = std::unique(v_.begin(), v_.end());
    v_.erase(last, v_.end());
    return v_;
}

int main() {
    hegel::hegel([]() {
        auto vec1 = hegel::draw(vectors(integers<int>()));
        auto vec2 = my_sort(vec1);
        std::sort(vec1.begin(), vec1.end());

        if (vec1 != vec2) {
            throw std::runtime_error("vectors not equal");
        }
    });

    return 0;
}
```

`hegel::hegel()` runs the lambda 100 times, each time with fresh random
values. If the lambda throws, Hegel records the failure and then replays
the test with progressively simpler inputs until it finds a minimal
counterexample.

For example, Hegel might initially find the following counterexample:
`[-2147483393,-749521737,-749521737]` which then becomes `[0, 0]` after shrinking.

## Running in a test suite

Each Hegel test is a standalone executable. You can integrate it with CTest
in the usual way:

```cmake
add_executable(test_sort test_sort.cpp)
target_link_libraries(test_sort PRIVATE hegel)
add_test(NAME test_sort COMMAND test_sort)
```

To run more or fewer test cases, to enable verbose output, or to set a seed:

```cpp
hegel::hegel([]() {
    // ...
}, {.test_cases = 500, .verbosity = hegel::Verbosity::Verbose, .seed = 1234});
```

## Generating multiple values

Use `hegel::draw()` to draw one value from a generator. Call it
multiple times to get independent values:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    auto a = hegel::draw(integers<int>({.min_value = 1, .max_value = 100}));
    auto b = hegel::draw(integers<int>({.min_value = 1, .max_value = 100}));
    auto s = hegel::draw(text({.min_size = 0, .max_size = 50}));

    // Use a, b, and s in your property check ...
});
```

You can also store a generator and reuse it:

```cpp
using namespace hegel::generators;

hegel::hegel([]() {
    auto small_int = integers<int>({.min_value = -10, .max_value = 10});

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
using namespace hegel::generators;

hegel::hegel([]() {
    auto nonzero = integers<int>({.min_value = -100, .max_value = 100})
        .filter([](int x) { return x != 0; });

    auto dividend = hegel::draw(integers<int>());
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
using namespace hegel::generators;

hegel::hegel([]() {
    // Generate even numbers by doubling
    auto even = integers<int>({.min_value = 0, .max_value = 50})
        .map([](int x) { return x * 2; });

    auto n = hegel::draw(even);

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

    auto s = hegel::draw(sized_string);

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
    auto vec = hegel::draw(vectors(integers<int>(), {.min_size = 1, .max_size = 20}));

    auto idx = hegel::draw(integers<size_t>({
        .min_value = 0,
        .max_value = static_cast<int64_t>(vec.size() - 1)
    }));

    // idx is always a valid index into vec
    auto element = vec.at(idx);
});
```

## What you can generate

All generator factory functions live in the `hegel::generators` namespace.
Import them with `using namespace hegel::generators;`.

### Primitive types

```cpp
auto b = hegel::draw(booleans());                          // bool
auto i = hegel::draw(integers<int>());                     // int (any integer type)
auto n = hegel::draw(integers<int64_t>({.min_value = 0})); // bounded int64_t
auto f = hegel::draw(floats<double>());                    // double
auto g = hegel::draw(floats<float>({                       // float with options
    .min_value = 0.0f,
    .max_value = 1.0f,
    .allow_nan = false,
    .allow_infinity = false
}));
auto s = hegel::draw(text());                              // std::string
auto t = hegel::draw(text({.min_size = 5, .max_size = 10}));
auto raw = hegel::draw(binary({.min_size = 16, .max_size = 16})); // vector<uint8_t>
```

### Constants and choices

```cpp
auto always_42 = hegel::draw(just(42));
auto color = hegel::draw(sampled_from({"red", "green", "blue"}));
auto die = hegel::draw(sampled_from({1, 2, 3, 4, 5, 6}));
```

### Collections

```cpp
auto vec = hegel::draw(vectors(integers<int>(), {.min_size = 1, .max_size = 10}));
auto unique_vec = hegel::draw(vectors(integers<int>(), {.unique = true}));
auto s = hegel::draw(sets(text(), {.min_size = 1, .max_size = 5}));
auto m = hegel::draw(dictionaries(text(), integers<int>(), {.max_size = 3}));
auto pair = hegel::draw(tuples(integers<int>(), text()));
```

### Random

```cpp
auto rng = hegel::draw(randoms());
std::uniform_real_distribution<double> dist(0.0, 10.0);
double uniform_value = dist(rng);

// Using true random
auto rng2 = hegel::draw(randoms({ .use_true_random = true }));
std::lognormal_distribution<double> dist2(0.0, 10.0);
double lognormal_value = dist2(rng2);
```

### Combinators

```cpp
// Choose from several generators
auto n = hegel::draw(one_of({
    integers<int>({.min_value = 0, .max_value = 10}),
    integers<int>({.min_value = 90, .max_value = 100})
}));

// Optional values (some or none)
auto opt = hegel::draw(optional_(text())); // std::optional<std::string>

// Variant types
auto var = hegel::draw(variant_(integers<int>(), text(), booleans()));
```

### Formats and addresses

```cpp
auto email = hegel::draw(emails());                        // e.g. "user@example.com"
auto url   = hegel::draw(urls());                          // e.g. "https://example.com/path"
auto dom   = hegel::draw(domains());                       // e.g. "sub.example.org"
auto ipv4  = hegel::draw(ip_addresses({.v = 4}));          // e.g. "192.168.1.1"
auto ipv6  = hegel::draw(ip_addresses({.v = 6}));          // e.g. "::1"
auto d     = hegel::draw(dates());                         // ISO 8601: "2024-03-15"
auto t     = hegel::draw(times());                         // "14:30:00"
auto dt    = hegel::draw(datetimes());                     // "2024-03-15T14:30:00"
auto pat   = hegel::draw(from_regex("[A-Z]{2}-[0-9]{4}")); // e.g. "QX-8271"
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
        auto p = hegel::draw(hegel::generators::default_generator<Point>());

        // p.x and p.y are random doubles
    });

    return 0;
}
```

You can also let Hegel derive a generator automatically, but override some of the 
default generators.

```cpp

int main() {
    hegel::hegel([]() {
        auto p = hegel::draw(hegel::generators::default_generator<Point>());
        // todo override
        // p.x and p.y are random doubles
    });

    return 0;
}
```

For explicitly defining the struct generator, use `builds` for
positional construction or `builds_agg` for named-field construction:

```cpp
using namespace hegel::generators;

struct Rectangle {
    int width;
    int height;
};

hegel::hegel([]() {
    // Positional: calls Rectangle{width, height}
    auto rect = hegel::draw(builds<Rectangle>(
        integers<int>({.min_value = 1, .max_value = 100}),
        integers<int>({.min_value = 1, .max_value = 100})
    ));

    // Named fields: explicit binding to struct members
    auto rect2 = hegel::draw(builds_agg<Rectangle>(
        field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value = 50})),
        field<&Rectangle::height>(integers<int>({.min_value = 1, .max_value = 50}))
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

using namespace hegel::generators;

int main() {
    hegel::hegel([]() {
        auto ops = hegel::draw(vectors(
            sampled_from({"push", "pop", "peek"}),
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
    using namespace hegel::generators;
    auto a = hegel::draw(integers<int>());
    auto b = hegel::draw(integers<int>());

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

- Browse [tests/property_tests.cpp](../tests/property_tests.cpp) to see example property-based tests.
- Read the generated Doxygen documentation (`just docs`) for the full API reference.
- See the [hegel-core](https://github.com/hegeldev/hegel-core) repository for details on the Hegel server 
  and the Hegel [website](https://hegel.dev/reference/protocol) for details on the binary protocol.
