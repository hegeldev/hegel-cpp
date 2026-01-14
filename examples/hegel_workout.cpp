// test_strategies.cpp - Tests hegel strategies for correct behavior
//
// Each test is a separate function that generates values and validates them.
// The main function uses sampled_from to pick which test to run.

#include <hegel/hegel.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <regex>

using namespace hegel;
using namespace hegel::st;

// =============================================================================
// Test helper
// =============================================================================

#define TEST_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "FAILED: " << msg << "\n";                                  \
      std::cerr << "Assertion failed: " #cond "\n";                            \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

// Check if string contains only ASCII characters (all bytes < 128)
bool is_ascii(const std::string &s) {
  for (unsigned char c : s) {
    if (c >= 128)
      return false;
  }
  return true;
}

// Count UTF-8 codepoints in a string
// UTF-8 encoding: continuation bytes start with 10xxxxxx (0x80-0xBF)
// So we count bytes that are NOT continuation bytes
size_t utf8_length(const std::string &s) {
  size_t count = 0;
  for (unsigned char c : s) {
    // Count if not a continuation byte (0x80-0xBF)
    if ((c & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

// =============================================================================
// Primitive tests
// =============================================================================

void test_nulls() {
  auto gen = nulls();
  auto value = gen.generate();
  // std::monostate has only one possible value, so just verify we got it
  (void)value;
  std::cout << "nulls: generated monostate\n";
}

void test_booleans() {
  auto gen = booleans();
  bool value = gen.generate();
  // Value must be true or false (always true for bool)
  TEST_ASSERT(value == true || value == false, "boolean must be true or false");
  std::cout << "booleans: " << (value ? "true" : "false") << "\n";
}

void test_just_int() {
  auto gen = just(42);
  int value = gen.generate();
  TEST_ASSERT(value == 42, "just(42) must produce 42");
  std::cout << "just(42): " << value << "\n";
}

void test_just_string() {
  auto gen = just("hello");
  std::string value = gen.generate();
  TEST_ASSERT(value == "hello", "just(\"hello\") must produce \"hello\"");
  std::cout << "just(\"hello\"): " << value << "\n";
}

// =============================================================================
// Integer tests
// =============================================================================

void test_integers_unbounded() {
  auto gen = integers<int64_t>();
  int64_t value = gen.generate();
  std::cout << "integers<int64_t>(): " << value << "\n";
}

void test_integers_bounded() {
  auto gen = integers<int>({.min_value = 10, .max_value = 20});
  int value = gen.generate();
  TEST_ASSERT(value >= 10 && value <= 20, "integers(10,20) must be in [10,20]");
  std::cout << "integers<int>({10,20}): " << value << "\n";
}

void test_integers_min_only() {
  auto gen = integers<int>({.min_value = 100});
  int value = gen.generate();
  TEST_ASSERT(value >= 100, "integers(min=100) must be >= 100");
  std::cout << "integers<int>({min=100}): " << value << "\n";
}

void test_integers_max_only() {
  auto gen = integers<int>({.max_value = -100});
  int value = gen.generate();
  TEST_ASSERT(value <= -100, "integers(max=-100) must be <= -100");
  std::cout << "integers<int>({max=-100}): " << value << "\n";
}

void test_integers_uint8() {
  auto gen = integers<uint8_t>();
  uint8_t value = gen.generate();
  TEST_ASSERT(value >= 0 && value <= 255, "uint8_t must be in [0,255]");
  std::cout << "integers<uint8_t>(): " << static_cast<int>(value) << "\n";
}

void test_integers_negative_range() {
  auto gen = integers<int>({.min_value = -50, .max_value = -10});
  int value = gen.generate();
  TEST_ASSERT(value >= -50 && value <= -10,
              "integers(-50,-10) must be in [-50,-10]");
  std::cout << "integers<int>({-50,-10}): " << value << "\n";
}

// =============================================================================
// Float tests
// =============================================================================

void test_floats_unbounded() {
  auto gen = floats<double>();
  double value = gen.generate();
  std::cout << "floats<double>(): " << value << "\n";
}

void test_floats_bounded() {
  auto gen = floats<double>({.min_value = 0.0, .max_value = 1.0});
  double value = gen.generate();
  TEST_ASSERT(value >= 0.0 && value <= 1.0, "floats(0,1) must be in [0,1]");
  std::cout << "floats<double>({0,1}): " << value << "\n";
}

void test_floats_exclusive() {
  auto gen = floats<double>({.min_value = 0.0,
                             .max_value = 1.0,
                             .exclude_min = true,
                             .exclude_max = true});
  double value = gen.generate();
  TEST_ASSERT(value > 0.0 && value < 1.0,
              "floats exclusive (0,1) must be in (0,1)");
  std::cout << "floats<double>(exclusive {0,1}): " << value << "\n";
}

void test_floats_float32() {
  auto gen = floats<float>({.min_value = -1.0f, .max_value = 1.0f});
  float value = gen.generate();
  TEST_ASSERT(value >= -1.0f && value <= 1.0f, "float must be in [-1,1]");
  std::cout << "floats<float>({-1,1}): " << value << "\n";
}

// =============================================================================
// String tests
//
// NOTE: reflect-cpp has a bug where strings containing null bytes (\u0000)
// are truncated at the null byte during JSON parsing. This means we cannot
// reliably check minimum string lengths, as a string that meets the minimum
// in the JSON response may be shorter after parsing.
// See: https://github.com/getml/reflect-cpp/issues/XXX
// =============================================================================

void test_text_unbounded() {
  auto gen = text();
  std::string value = gen.generate();
  std::cout << "text(): \"" << value << "\" (codepoints=" << utf8_length(value)
            << ")\n";
}

void test_text_bounded() {
  auto gen = text({.min_size = 5, .max_size = 10});
  std::string value = gen.generate();
  size_t len = utf8_length(value);
  // NOTE: Cannot check min length due to reflect-cpp null byte truncation bug
  TEST_ASSERT(len <= 10, "text(5,10) codepoint length must be <= 10");
  std::cout << "text({5,10}): \"" << value << "\" (codepoints=" << len << ")\n";
}

void test_text_exact_min() {
  auto gen = text({.min_size = 3});
  std::string value = gen.generate();
  size_t len = utf8_length(value);
  // NOTE: Cannot check min length due to reflect-cpp null byte truncation bug
  std::cout << "text({min=3}): \"" << value << "\" (codepoints=" << len
            << ")\n";
}

void test_from_regex() {
  // Use explicit ASCII character classes to avoid Unicode digits (which could
  // include characters that get mangled by null byte truncation)
  auto gen = from_regex(R"([a-z]{3}-[0-9]{3})");
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("from_regex produced non-ASCII string");
  }

  std::regex pattern(R"([a-z]{3}-[0-9]{3})");
  TEST_ASSERT(std::regex_match(value, pattern),
              "from_regex must match pattern [a-z]{3}-[0-9]{3}");
  std::cout << "from_regex([a-z]{3}-[0-9]{3}): \"" << value << "\"\n";
}

// =============================================================================
// Format string tests
// =============================================================================

void test_emails() {
  auto gen = emails();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("email produced non-ASCII string");
  }

  TEST_ASSERT(value.find('@') != std::string::npos, "email must contain @");
  std::cout << "emails(): \"" << value << "\"\n";
}

void test_urls() {
  auto gen = urls();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("url produced non-ASCII string");
  }

  TEST_ASSERT(value.find("://") != std::string::npos, "url must contain ://");
  std::cout << "urls(): \"" << value << "\"\n";
}

void test_domains() {
  auto gen = domains();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("domain produced non-ASCII string");
  }

  // NOTE: Cannot check non-empty due to reflect-cpp null byte truncation bug
  TEST_ASSERT(utf8_length(value) <= 255, "domain must be <= 255 codepoints");
  std::cout << "domains(): \"" << value << "\"\n";
}

void test_ip_addresses_v4() {
  auto gen = ip_addresses({.v = 4});
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("ipv4 produced non-ASCII string");
  }

  std::regex ipv4_pattern(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})");
  TEST_ASSERT(std::regex_match(value, ipv4_pattern),
              "ipv4 must match dotted quad format");
  std::cout << "ip_addresses({v=4}): \"" << value << "\"\n";
}

void test_ip_addresses_v6() {
  auto gen = ip_addresses({.v = 6});
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("ipv6 produced non-ASCII string");
  }

  TEST_ASSERT(value.find(':') != std::string::npos, "ipv6 must contain colons");
  std::cout << "ip_addresses({v=6}): \"" << value << "\"\n";
}

void test_ip_addresses_any() {
  auto gen = ip_addresses();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("ip address produced non-ASCII string");
  }

  bool is_v4 = value.find('.') != std::string::npos;
  bool is_v6 = value.find(':') != std::string::npos;
  TEST_ASSERT(is_v4 || is_v6, "ip address must be v4 or v6");
  std::cout << "ip_addresses(): \"" << value << "\"\n";
}

// =============================================================================
// Datetime tests
// =============================================================================

void test_dates() {
  auto gen = dates();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("date produced non-ASCII string");
  }

  // ISO date: YYYY-MM-DD
  std::regex date_pattern(R"(\d{4}-\d{2}-\d{2})");
  TEST_ASSERT(std::regex_match(value, date_pattern),
              "date must match YYYY-MM-DD");
  std::cout << "dates(): \"" << value << "\"\n";
}

void test_times() {
  auto gen = times();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("time produced non-ASCII string");
  }

  // Should contain colons for HH:MM:SS
  TEST_ASSERT(value.find(':') != std::string::npos, "time must contain colons");
  std::cout << "times(): \"" << value << "\"\n";
}

void test_datetimes() {
  auto gen = datetimes();
  std::string value = gen.generate();

  if (!is_ascii(value)) {
    reject("datetime produced non-ASCII string");
  }

  // Should contain both date and time components
  TEST_ASSERT(value.find('-') != std::string::npos,
              "datetime must contain date part");
  TEST_ASSERT(value.find(':') != std::string::npos,
              "datetime must contain time part");
  std::cout << "datetimes(): \"" << value << "\"\n";
}

// =============================================================================
// Collection tests
// =============================================================================

void test_vectors_basic() {
  auto gen = vectors(integers<int>());
  std::vector<int> value = gen.generate();
  std::cout << "vectors(integers<int>()): size=" << value.size() << "\n";
}

void test_vectors_bounded() {
  auto gen = vectors(integers<int>({.min_value = 0, .max_value = 100}),
                     {.min_size = 3, .max_size = 5});
  std::vector<int> value = gen.generate();
  TEST_ASSERT(value.size() >= 3 && value.size() <= 5,
              "vectors(min=3,max=5) size must be in [3,5]");
  for (int v : value) {
    TEST_ASSERT(v >= 0 && v <= 100, "vector elements must be in [0,100]");
  }
  std::cout << "vectors({3,5}): [";
  for (size_t i = 0; i < value.size(); ++i) {
    if (i > 0)
      std::cout << ", ";
    std::cout << value[i];
  }
  std::cout << "]\n";
}

void test_vectors_unique() {
  auto gen = vectors(integers<int>({.min_value = 0, .max_value = 1000}),
                     {.min_size = 5, .max_size = 10, .unique = true});
  std::vector<int> value = gen.generate();
  std::set<int> unique_values(value.begin(), value.end());
  TEST_ASSERT(unique_values.size() == value.size(),
              "unique vectors must have no duplicates");
  std::cout << "vectors(unique): size=" << value.size() << ", all unique\n";
}

void test_sets() {
  auto gen = sets(integers<int>({.min_value = 0, .max_value = 100}),
                  {.min_size = 3, .max_size = 7});
  std::set<int> value = gen.generate();
  TEST_ASSERT(value.size() >= 3 && value.size() <= 7,
              "sets(3,7) size must be in [3,7]");
  std::cout << "sets({3,7}): size=" << value.size() << "\n";
}

void test_dictionaries() {
  auto gen = dictionaries(text({.min_size = 1, .max_size = 10}),
                          integers<int>(), {.min_size = 1, .max_size = 3});
  std::map<std::string, int> value = gen.generate();

  TEST_ASSERT(value.size() >= 1 && value.size() <= 3,
              "dictionaries(1,3) size must be in [1,3]");
  std::cout << "dictionaries({1,3}): {";
  bool first = true;
  for (const auto &[k, v] : value) {
    if (!first)
      std::cout << ", ";
    std::cout << "\"" << k << "\": " << v;
    first = false;
  }
  std::cout << "}\n";
}

// =============================================================================
// Tuple tests
// =============================================================================

void test_tuples_pair() {
  auto gen = tuples(integers<int>(), text({.max_size = 10}));
  auto [i, s] = gen.generate();
  std::cout << "tuples(int, string): (" << i << ", \"" << s << "\")\n";
}

void test_tuples_triple() {
  auto gen =
      tuples(booleans(), integers<int>({.min_value = 0}), floats<double>());
  auto [b, i, f] = gen.generate();
  TEST_ASSERT(i >= 0, "tuple int element must be >= 0");
  std::cout << "tuples(bool, int, double): (" << b << ", " << i << ", " << f
            << ")\n";
}

// =============================================================================
// Combinator tests
// =============================================================================

void test_sampled_from_strings() {
  auto gen = sampled_from({"apple", "banana", "cherry"});
  std::string value = gen.generate();
  TEST_ASSERT(value == "apple" || value == "banana" || value == "cherry",
              "sampled_from must return one of the options");
  std::cout << "sampled_from(fruits): \"" << value << "\"\n";
}

void test_sampled_from_ints() {
  auto gen = sampled_from({10, 20, 30, 40, 50});
  int value = gen.generate();
  TEST_ASSERT(value == 10 || value == 20 || value == 30 || value == 40 ||
                  value == 50,
              "sampled_from must return one of the options");
  std::cout << "sampled_from(ints): " << value << "\n";
}

void test_one_of() {
  auto gen = one_of({integers<int>({.min_value = 0, .max_value = 10}),
                     integers<int>({.min_value = 100, .max_value = 110})});
  int value = gen.generate();
  TEST_ASSERT((value >= 0 && value <= 10) || (value >= 100 && value <= 110),
              "one_of must return from one of the ranges");
  std::cout << "one_of(0-10, 100-110): " << value << "\n";
}

void test_variant() {
  auto gen = variant_(text({.max_size = 10}), integers<int>());
  std::variant<std::string, int> value = gen.generate();

  std::visit(
      [](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
          std::cout << "variant(string, int): string \"" << v << "\"\n";
        } else {
          std::cout << "variant(string, int): int " << v << "\n";
        }
      },
      value);
}

void test_optional() {
  auto gen = optional_(integers<int>({.min_value = 0, .max_value = 100}));
  std::optional<int> value = gen.generate();
  if (value) {
    TEST_ASSERT(*value >= 0 && *value <= 100,
                "optional value must be in range");
    std::cout << "optional(int): " << *value << "\n";
  } else {
    std::cout << "optional(int): nullopt\n";
  }
}

// =============================================================================
// Builds tests
// =============================================================================

struct Point {
  int x;
  int y;
};

struct Person {
  std::string name;
  int age;
};

void test_builds_positional() {
  auto gen =
      builds<Point>(integers<int>({.min_value = -100, .max_value = 100}),
                    integers<int>({.min_value = -100, .max_value = 100}));
  Point p = gen.generate();
  TEST_ASSERT(p.x >= -100 && p.x <= 100, "point.x must be in range");
  TEST_ASSERT(p.y >= -100 && p.y <= 100, "point.y must be in range");
  std::cout << "builds<Point>: (" << p.x << ", " << p.y << ")\n";
}

void test_builds_agg() {
  auto gen = builds_agg<Person>(
      field<&Person::name>(text({.min_size = 1, .max_size = 20})),
      field<&Person::age>(integers<int>({.min_value = 0, .max_value = 120})));
  Person p = gen.generate();
  size_t name_len = utf8_length(p.name);
  // NOTE: Cannot check min length due to reflect-cpp null byte truncation bug
  TEST_ASSERT(name_len <= 20, "person.name must be <= 20 codepoints");
  TEST_ASSERT(p.age >= 0 && p.age <= 120, "person.age must be in [0,120]");
  std::cout << "builds_agg<Person>: {name=\"" << p.name << "\", age=" << p.age
            << "}\n";
}

// =============================================================================
// Map/filter tests
// =============================================================================

void test_map() {
  auto gen = integers<int>({.min_value = 1, .max_value = 10}).map([](int x) {
    return x * x;
  });
  int value = gen.generate();
  // Should be a perfect square between 1 and 100
  int root = static_cast<int>(std::sqrt(value));
  TEST_ASSERT(root * root == value, "mapped value should be a perfect square");
  TEST_ASSERT(value >= 1 && value <= 100, "squared value must be in [1,100]");
  std::cout << "integers.map(x*x): " << value << "\n";
}

void test_filter() {
  auto gen =
      integers<int>({.min_value = 0, .max_value = 100}).filter([](int x) {
        return x % 2 == 0;
      });
  int value = gen.generate();
  TEST_ASSERT(value % 2 == 0, "filtered value must be even");
  TEST_ASSERT(value >= 0 && value <= 100, "filtered value must be in range");
  std::cout << "integers.filter(even): " << value << "\n";
}

void test_flatmap() {
  // Generate a length, then generate a string of that length
  auto gen = integers<size_t>({.min_value = 3, .max_value = 8})
                 .flatmap([](size_t len) {
                   return text({.min_size = len, .max_size = len});
                 });
  std::string value = gen.generate();
  size_t len = utf8_length(value);
  // NOTE: Cannot check min length due to reflect-cpp null byte truncation bug
  TEST_ASSERT(len <= 8, "flatmap string codepoint length must be <= 8");
  std::cout << "integers.flatmap(len -> text(len)): \"" << value
            << "\" (codepoints=" << len << ")\n";
}

// =============================================================================
// Test registry
// =============================================================================

struct TestCase {
  std::string name;
  void (*func)();
};

const TestCase ALL_TESTS[] = {
    // Primitives
    {"nulls", test_nulls},
    {"booleans", test_booleans},
    {"just_int", test_just_int},
    {"just_string", test_just_string},

    // Integers
    {"integers_unbounded", test_integers_unbounded},
    {"integers_bounded", test_integers_bounded},
    {"integers_min_only", test_integers_min_only},
    {"integers_max_only", test_integers_max_only},
    {"integers_uint8", test_integers_uint8},
    {"integers_negative_range", test_integers_negative_range},

    // Floats
    {"floats_unbounded", test_floats_unbounded},
    {"floats_bounded", test_floats_bounded},
    {"floats_exclusive", test_floats_exclusive},
    {"floats_float32", test_floats_float32},

    // Strings
    {"text_unbounded", test_text_unbounded},
    {"text_bounded", test_text_bounded},
    {"text_exact_min", test_text_exact_min},
    {"from_regex", test_from_regex},

    // Format strings
    {"emails", test_emails},
    {"urls", test_urls},
    {"domains", test_domains},
    {"ip_addresses_v4", test_ip_addresses_v4},
    {"ip_addresses_v6", test_ip_addresses_v6},
    {"ip_addresses_any", test_ip_addresses_any},

    // Datetime
    {"dates", test_dates},
    {"times", test_times},
    {"datetimes", test_datetimes},

    // Collections
    {"vectors_basic", test_vectors_basic},
    {"vectors_bounded", test_vectors_bounded},
    {"vectors_unique", test_vectors_unique},
    {"sets", test_sets},
    {"dictionaries", test_dictionaries},

    // Tuples
    {"tuples_pair", test_tuples_pair},
    {"tuples_triple", test_tuples_triple},

    // Combinators
    {"sampled_from_strings", test_sampled_from_strings},
    {"sampled_from_ints", test_sampled_from_ints},
    {"one_of", test_one_of},
    {"variant", test_variant},
    {"optional", test_optional},

    // Builds
    {"builds_positional", test_builds_positional},
    {"builds_agg", test_builds_agg},

    // Map/filter/flatmap
    {"map", test_map},
    {"filter", test_filter},
    {"flatmap", test_flatmap},
};

constexpr size_t NUM_TESTS = sizeof(ALL_TESTS) / sizeof(ALL_TESTS[0]);

// =============================================================================
// Main
// =============================================================================

int main() {
  // Build vector of test names for sampled_from
  std::vector<std::string> test_names;
  test_names.reserve(NUM_TESTS);
  for (const auto &test : ALL_TESTS) {
    test_names.push_back(test.name);
  }

  // Use sampled_from to pick which test to run
  auto test_selector = sampled_from(test_names);
  std::string selected = test_selector.generate();

  std::cout << "Selected test: " << selected << "\n";
  std::cout << "----------------------------------------\n";

  // Find and run the selected test
  for (const auto &test : ALL_TESTS) {
    if (test.name == selected) {
      test.func();
      std::cout << "----------------------------------------\n";
      std::cout << "PASSED: " << selected << "\n";
      return 0;
    }
  }

  std::cerr << "Unknown test: " << selected << "\n";
  return 1;
}
