# Hegel C++ SDK

Hypothesis-like testing for C++20.

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
#include "hegel.hpp"

// Define a struct - reflect-cpp will derive the schema
struct Person {
    std::string name;
    int age;
};

int main() {
    // Generate a Person with automatic schema derivation
    auto gen = hegel::default_generator<Person>();
    Person p = gen.generate();

    // Or use explicit strategies
    using namespace hegel::st;
    auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
    int value = int_gen.generate();

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

auto gen = hegel::default_generator<Point>();
Point p = gen.generate();
```

### Strategies

All strategies are in the `hegel::st` namespace.

#### Primitives

```cpp
using namespace hegel::st;

// Null values
auto null_gen = nulls();

// Booleans
auto bool_gen = booleans();

// Constant values
auto const_gen = just(42);
auto str_gen = just("hello");
```

#### Numbers

```cpp
// Integers with optional bounds (uses type limits by default)
auto int_gen = integers<int>();
auto bounded_int = integers<int>({.min_value = 0, .max_value = 100});
auto min_only = integers<int>({.min_value = 0});

// Floating point
auto float_gen = floats<double>();
auto bounded_float = floats<double>({
    .min_value = 0.0,
    .max_value = 1.0,
    .exclude_min = true,  // exclusive bounds
    .exclude_max = true
});
```

#### Strings

```cpp
// Text with optional length constraints
auto text_gen = text();
auto bounded_text = text({.min_size = 1, .max_size = 100});

// Regex patterns
auto pattern_gen = from_regex("[a-z]{3}-[0-9]{3}");

// Format strings
auto email_gen = emails();
auto url_gen = urls();
auto domain_gen = domains({.max_length = 63});
auto ip_gen = ip_addresses();          // IPv4 or IPv6
auto ipv4_gen = ip_addresses({.v = 4}); // IPv4 only
auto ipv6_gen = ip_addresses({.v = 6}); // IPv6 only

// Date/time (ISO 8601)
auto date_gen = dates();      // YYYY-MM-DD
auto time_gen = times();      // HH:MM:SS
auto datetime_gen = datetimes();
```

#### Collections

```cpp
// Vectors
auto vec_gen = vectors(integers<int>());
auto bounded_vec = vectors(integers<int>(), {
    .min_size = 1,
    .max_size = 10,
    .unique = true  // no duplicates
});

// Sets
auto set_gen = sets(integers<int>(), {.min_size = 1, .max_size = 5});

// Dictionaries (string keys only, JSON limitation)
auto dict_gen = dictionaries(text(), integers<int>(), {
    .min_size = 1,
    .max_size = 3
});
```

#### Tuples

```cpp
auto pair_gen = tuples(integers<int>(), text());
auto triple_gen = tuples(booleans(), integers<int>(), floats<double>());
```

#### Combinators

```cpp
// Sample from fixed set
auto color_gen = sampled_from({"red", "green", "blue"});
auto num_gen = sampled_from({1, 2, 3, 4, 5});

// Choose from multiple generators
auto range_gen = one_of({
    integers<int>({.min_value = 0, .max_value = 10}),
    integers<int>({.min_value = 100, .max_value = 110})
});

// Optional values
auto opt_gen = optional_(integers<int>());

// Variants
auto var_gen = variant_(integers<int>(), text(), booleans());
```

### Combinators: map, flatmap, filter

```cpp
// Transform values
auto squared = integers<int>({.min_value = 1, .max_value = 10})
    .map([](int x) { return x * x; });

// Filter values (rejects after max_attempts failures)
auto even = integers<int>({.min_value = 0, .max_value = 100})
    .filter([](int x) { return x % 2 == 0; }, 10);

// Dependent generation
auto sized_string = integers<size_t>({.min_value = 1, .max_value = 10})
    .flatmap([](size_t len) {
        return text({.min_size = len, .max_size = len});
    });
```

### Object Construction

```cpp
struct Rectangle {
    int width;
    int height;
};

// Positional construction (calls constructor)
auto rect_gen = builds<Rectangle>(
    integers<int>({.min_value = 1, .max_value = 100}),
    integers<int>({.min_value = 1, .max_value = 100})
);

// Named aggregate initialization
auto rect_gen2 = builds_agg<Rectangle>(
    field<&Rectangle::width>(integers<int>({.min_value = 1, .max_value = 100})),
    field<&Rectangle::height>(integers<int>({.min_value = 1, .max_value = 100}))
);
```

### Assumptions

Use `assume()` when generated data doesn't meet preconditions:

```cpp
auto person = person_gen.generate();
hegel::assume(person.age >= 18);
```

This tells Hegel to try different input data rather than counting as a test failure.
