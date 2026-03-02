// property_tests.cpp - Property-based unit tests using hegel in embedded mode
//
// These tests demonstrate standard property-based testing patterns:
// - Algebraic properties (commutativity, associativity, identity)
// - Invariants (sorting, data structure properties)
// - Round-trip properties (encode/decode, serialize/deserialize)
// - Bounds and size properties
//
// Run with: ctest or ./property_tests

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <hegel/hegel.h>
#include <numeric>
#include <set>
#include <sstream>

using namespace hegel::generators;
using hegel::assume;
using hegel::draw;
using hegel::note;

// =============================================================================
// Algebraic Properties
// =============================================================================

TEST(AlgebraicProperties, AdditionIsCommutative) {
    hegel::hegel([] {
        auto x = draw(integers<int32_t>());
        auto y = draw(integers<int32_t>());
        note("Testing: " + std::to_string(x) + " + " + std::to_string(y));

        // Use wrapping arithmetic to avoid undefined behavior
        int32_t lhs = x + y;
        int32_t rhs = y + x;
        ASSERT_EQ(lhs, rhs) << "Addition should be commutative";
    });
}

TEST(AlgebraicProperties, MultiplicationIsCommutative) {
    hegel::hegel([] {
        auto x = draw(integers<int32_t>());
        auto y = draw(integers<int32_t>());
        note("Testing: " + std::to_string(x) + " * " + std::to_string(y));

        int32_t lhs = x * y;
        int32_t rhs = y * x;
        ASSERT_EQ(lhs, rhs) << "Multiplication should be commutative";
    });
}

TEST(AlgebraicProperties, AdditionIsAssociative) {
    hegel::hegel([] {
        // Use smaller integers to avoid overflow
        auto gen = integers<int16_t>();
        auto x = draw(gen);
        auto y = draw(gen);
        auto z = draw(gen);
        note("Testing: (" + std::to_string(x) + " + " + std::to_string(y) +
             ") + " + std::to_string(z));

        // Cast to int32_t to avoid overflow during computation
        int32_t lhs = (static_cast<int32_t>(x) + y) + z;
        int32_t rhs = static_cast<int32_t>(x) + (y + z);
        ASSERT_EQ(lhs, rhs) << "Addition should be associative";
    });
}

TEST(AlgebraicProperties, ZeroIsAdditiveIdentity) {
    hegel::hegel([] {
        auto x = draw(integers<int64_t>());
        note("Testing: " + std::to_string(x) + " + 0");

        ASSERT_EQ(x + 0, x) << "Zero should be additive identity";
        ASSERT_EQ(0 + x, x) << "Zero should be additive identity";
    });
}

TEST(AlgebraicProperties, OneIsMultiplicativeIdentity) {
    hegel::hegel([] {
        auto x = draw(integers<int64_t>());
        note("Testing: " + std::to_string(x) + " * 1");

        ASSERT_EQ(x * 1, x) << "One should be multiplicative identity";
        ASSERT_EQ(1 * x, x) << "One should be multiplicative identity";
    });
}

// =============================================================================
// Sorting Invariants
// =============================================================================

TEST(SortingInvariants, SortedOutputIsSorted) {
    hegel::hegel([] {
        auto v =
            draw(vectors(integers<int>(), {.min_size = 0, .max_size = 100}));
        note("Testing vector of size " + std::to_string(v.size()));

        std::vector<int> sorted = v;
        std::sort(sorted.begin(), sorted.end());

        ASSERT_TRUE(std::is_sorted(sorted.begin(), sorted.end()))
            << "Sorted output should be sorted";
    });
}

TEST(SortingInvariants, SortPreservesLength) {
    hegel::hegel([] {
        auto v =
            draw(vectors(integers<int>(), {.min_size = 0, .max_size = 100}));
        size_t original_size = v.size();

        std::sort(v.begin(), v.end());

        ASSERT_EQ(v.size(), original_size)
            << "Sorting should preserve vector length";
    });
}

TEST(SortingInvariants, SortPreservesElements) {
    hegel::hegel([] {
        auto v =
            draw(vectors(integers<int>(), {.min_size = 0, .max_size = 50}));
        std::multiset<int> original(v.begin(), v.end());

        std::sort(v.begin(), v.end());

        std::multiset<int> sorted(v.begin(), v.end());
        ASSERT_EQ(original, sorted) << "Sorting should preserve all elements";
    });
}

TEST(SortingInvariants, SortIsIdempotent) {
    hegel::hegel([] {
        auto v =
            draw(vectors(integers<int>(), {.min_size = 0, .max_size = 50}));

        std::sort(v.begin(), v.end());
        std::vector<int> once_sorted = v;

        std::sort(v.begin(), v.end());

        ASSERT_EQ(v, once_sorted) << "Sorting twice should equal sorting once";
    });
}

// =============================================================================
// String Properties
// =============================================================================

TEST(StringProperties, ReverseReverseIsIdentity) {
    hegel::hegel([] {
        auto s = draw(text({.max_size = 100}));
        note("Testing string of length " + std::to_string(s.size()));

        std::string reversed = s;
        std::reverse(reversed.begin(), reversed.end());
        std::reverse(reversed.begin(), reversed.end());

        ASSERT_EQ(reversed, s) << "Reversing twice should give original";
    });
}

TEST(StringProperties, ConcatenationLengthIsSum) {
    hegel::hegel([] {
        auto s1 = draw(text({.max_size = 50}));
        auto s2 = draw(text({.max_size = 50}));

        std::string concatenated = s1 + s2;

        ASSERT_EQ(concatenated.size(), s1.size() + s2.size())
            << "Concatenation length should be sum of lengths";
    });
}

TEST(StringProperties, SubstringIsContained) {
    hegel::hegel([] {
        auto s = draw(text({.min_size = 5, .max_size = 100}));
        assume(s.size() >= 5);

        auto start = draw(
            integers<size_t>({.min_value = 0, .max_value = s.size() - 1}));
        auto len = draw(
            integers<size_t>({.min_value = 1, .max_value = s.size() - start}));

        std::string sub = s.substr(start, len);

        ASSERT_NE(s.find(sub), std::string::npos)
            << "Substring should be found in original";
    });
}

// =============================================================================
// Collection Properties
// =============================================================================

TEST(CollectionProperties, SetHasNoDuplicates) {
    hegel::hegel([] {
        auto s = draw(sets(integers<int>({.min_value = 0, .max_value = 1000}),
                           {.min_size = 0, .max_size = 50}));

        std::vector<int> elements(s.begin(), s.end());
        std::set<int> unique(elements.begin(), elements.end());

        ASSERT_EQ(elements.size(), unique.size())
            << "Set should have no duplicates";
    });
}

TEST(CollectionProperties, UniqueVectorHasNoDuplicates) {
    hegel::hegel([] {
        auto v = draw(
            vectors(integers<int>({.min_value = 0, .max_value = 10000}),
                    {.min_size = 0, .max_size = 100, .unique = true}));

        std::set<int> unique(v.begin(), v.end());

        ASSERT_EQ(v.size(), unique.size())
            << "Unique vector should have no duplicates";
    });
}

TEST(CollectionProperties, MapKeysAreUnique) {
    hegel::hegel([] {
        auto m =
            draw(dictionaries(text({.min_size = 1, .max_size = 20}),
                              integers<int>(), {.min_size = 0, .max_size = 20}));

        std::vector<std::string> keys;
        for (const auto& [k, v] : m) {
            keys.push_back(k);
        }
        std::set<std::string> unique_keys(keys.begin(), keys.end());

        ASSERT_EQ(keys.size(), unique_keys.size())
            << "Map keys should be unique";
    });
}

TEST(CollectionProperties, VectorSumIsOrderIndependent) {
    hegel::hegel([] {
        auto v = draw(
            vectors(integers<int>({.min_value = -1000, .max_value = 1000}),
                    {.min_size = 0, .max_size = 50}));

        int64_t sum1 = std::accumulate(v.begin(), v.end(), int64_t{0});

        std::vector<int> shuffled = v;
        std::sort(shuffled.begin(), shuffled.end());
        std::reverse(shuffled.begin(), shuffled.end());

        int64_t sum2 =
            std::accumulate(shuffled.begin(), shuffled.end(), int64_t{0});

        ASSERT_EQ(sum1, sum2) << "Sum should be order-independent";
    });
}

// =============================================================================
// Numeric Properties
// =============================================================================

TEST(NumericProperties, AbsoluteValueIsNonNegative) {
    hegel::hegel([] {
        auto x = draw(integers<int32_t>());
        // Avoid INT_MIN which has no positive counterpart
        assume(x != std::numeric_limits<int32_t>::min());
        note("Testing abs(" + std::to_string(x) + ")");

        ASSERT_GE(std::abs(x), 0) << "Absolute value should be non-negative";
    });
}

TEST(NumericProperties, MaxIsGreaterOrEqual) {
    hegel::hegel([] {
        auto x = draw(integers<int>());
        auto y = draw(integers<int>());
        note("Testing max(" + std::to_string(x) + ", " + std::to_string(y) +
             ")");

        int m = std::max(x, y);
        ASSERT_GE(m, x) << "Max should be >= first argument";
        ASSERT_GE(m, y) << "Max should be >= second argument";
        ASSERT_TRUE(m == x || m == y) << "Max should equal one of the inputs";
    });
}

TEST(NumericProperties, MinIsLessOrEqual) {
    hegel::hegel([] {
        auto x = draw(integers<int>());
        auto y = draw(integers<int>());

        int m = std::min(x, y);
        ASSERT_LE(m, x) << "Min should be <= first argument";
        ASSERT_LE(m, y) << "Min should be <= second argument";
        ASSERT_TRUE(m == x || m == y) << "Min should equal one of the inputs";
    });
}

TEST(NumericProperties, ClampIsInRange) {
    hegel::hegel([] {
        auto lo = draw(integers<int>({.min_value = -100, .max_value = 0}));
        auto hi = draw(integers<int>({.min_value = 0, .max_value = 100}));
        auto x = draw(integers<int>());

        if (lo > hi)
            std::swap(lo, hi);

        int clamped = std::clamp(x, lo, hi);

        ASSERT_GE(clamped, lo) << "Clamped value should be >= lower bound";
        ASSERT_LE(clamped, hi) << "Clamped value should be <= upper bound";
    });
}

// =============================================================================
// Round-trip Properties
// =============================================================================

TEST(RoundTrip, IntToStringToInt) {
    hegel::hegel([] {
        auto x = draw(integers<int>());
        note("Testing round-trip for " + std::to_string(x));

        std::string s = std::to_string(x);
        int y = std::stoi(s);

        ASSERT_EQ(x, y) << "Int -> string -> int should be identity";
    });
}

TEST(RoundTrip, DoubleToStringToDouble) {
    hegel::hegel([] {
        // Use bounded floats to avoid special values
        auto x =
            draw(floats<double>({.min_value = -1e10, .max_value = 1e10}));

        // Skip special values that may be generated despite bounds
        if (std::isnan(x) || std::isinf(x)) {
            return; // Skip this test case
        }

        std::ostringstream oss;
        oss << std::setprecision(17) << x;
        std::string str = oss.str();

        // stod may fail for some edge cases
        double y;
        try {
            y = std::stod(str);
        } catch (const std::exception&) {
            return; // Skip unparseable cases
        }

        // Allow small relative error due to floating point representation
        if (x != 0.0) {
            double rel_error = std::abs((y - x) / x);
            ASSERT_LT(rel_error, 1e-14)
                << "Double round-trip should have minimal error";
        } else {
            ASSERT_EQ(y, 0.0) << "Zero should round-trip exactly";
        }
    });
}

// =============================================================================
// Data Structure Properties
// =============================================================================

struct Point {
    int x;
    int y;
};

TEST(DataStructureProperties, PointDistanceIsNonNegative) {
    hegel::hegel([] {
        auto gen = builds<Point>(
            integers<int>({.min_value = -1000, .max_value = 1000}),
            integers<int>({.min_value = -1000, .max_value = 1000}));
        auto p = draw(gen);
        note("Testing point (" + std::to_string(p.x) + ", " +
             std::to_string(p.y) + ")");

        double dist = std::sqrt(static_cast<double>(p.x) * p.x +
                                static_cast<double>(p.y) * p.y);

        ASSERT_GE(dist, 0.0) << "Distance from origin should be non-negative";
    });
}

TEST(DataStructureProperties, OptionalHasValueOrNot) {
    hegel::hegel([] {
        auto opt = draw(optional_(integers<int>()));

        // This is a tautology, but demonstrates optional generation
        ASSERT_TRUE(opt.has_value() || !opt.has_value())
            << "Optional must have value or not";

        if (opt) {
            note("Got value: " + std::to_string(*opt));
        } else {
            note("Got nullopt");
        }
    });
}

// =============================================================================
// Filter and Map Properties
// =============================================================================

TEST(FilterMapProperties, FilteredValuesMatchPredicate) {
    hegel::hegel([] {
        auto gen =
            integers<int>({.min_value = 0, .max_value = 100}).filter([](int x) {
                return x % 2 == 0;
            });
        auto x = draw(gen);

        ASSERT_EQ(x % 2, 0) << "Filtered values should be even";
        ASSERT_GE(x, 0) << "Value should be in original range";
        ASSERT_LE(x, 100) << "Value should be in original range";
    });
}

TEST(FilterMapProperties, MappedValuesAreTransformed) {
    hegel::hegel([] {
        auto gen =
            integers<int>({.min_value = 1, .max_value = 10}).map([](int x) {
                return x * x;
            });
        auto x = draw(gen);

        // Result should be a perfect square between 1 and 100
        int root = static_cast<int>(std::sqrt(x));
        ASSERT_EQ(root * root, x) << "Mapped value should be a perfect square";
        ASSERT_GE(x, 1) << "Value should be >= 1";
        ASSERT_LE(x, 100) << "Value should be <= 100";
    });
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
