#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <hegel/hegel.h>
#include <numeric>
#include <set>
#include <sstream>

namespace gs = hegel::generators;

// =============================================================================
// Algebraic Properties
// =============================================================================

TEST(AlgebraicProperties, AdditionIsCommutative) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int32_t>());
        auto y = tc.draw(gs::integers<int32_t>());
        tc.note("Testing: " + std::to_string(x) + " + " + std::to_string(y));

        // Use wrapping arithmetic to avoid undefined behavior
        int32_t lhs = x + y;
        int32_t rhs = y + x;
        ASSERT_EQ(lhs, rhs) << "Addition should be commutative";
    });
}

TEST(AlgebraicProperties, MultiplicationIsCommutative) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int32_t>());
        auto y = tc.draw(gs::integers<int32_t>());
        tc.note("Testing: " + std::to_string(x) + " * " + std::to_string(y));

        int32_t lhs = x * y;
        int32_t rhs = y * x;
        ASSERT_EQ(lhs, rhs) << "Multiplication should be commutative";
    });
}

TEST(AlgebraicProperties, AdditionIsAssociative) {
    hegel::test([](hegel::TestCase& tc) {
        // Use smaller integers to avoid overflow
        auto gen = gs::integers<int16_t>();
        auto x = tc.draw(gen);
        auto y = tc.draw(gen);
        auto z = tc.draw(gen);
        tc.note("Testing: (" + std::to_string(x) + " + " + std::to_string(y) +
                ") + " + std::to_string(z));

        // Cast to int32_t to avoid overflow during computation
        int32_t lhs = (static_cast<int32_t>(x) + y) + z;
        int32_t rhs = static_cast<int32_t>(x) + (y + z);
        ASSERT_EQ(lhs, rhs) << "Addition should be associative";
    });
}

TEST(AlgebraicProperties, ZeroIsAdditiveIdentity) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int64_t>());
        tc.note("Testing: " + std::to_string(x) + " + 0");

        ASSERT_EQ(x + 0, x) << "Zero should be additive identity";
        ASSERT_EQ(0 + x, x) << "Zero should be additive identity";
    });
}

TEST(AlgebraicProperties, OneIsMultiplicativeIdentity) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int64_t>());
        tc.note("Testing: " + std::to_string(x) + " * 1");

        ASSERT_EQ(x * 1, x) << "One should be multiplicative identity";
        ASSERT_EQ(1 * x, x) << "One should be multiplicative identity";
    });
}

// =============================================================================
// Sorting Invariants
// =============================================================================

TEST(SortingInvariants, SortedOutputIsSorted) {
    hegel::test([](hegel::TestCase& tc) {
        auto v = tc.draw(
            gs::vectors(gs::integers<int>(), {.min_size = 0, .max_size = 100}));
        tc.note("Testing vector of size " + std::to_string(v.size()));

        std::vector<int> sorted = v;
        std::sort(sorted.begin(), sorted.end());

        ASSERT_TRUE(std::is_sorted(sorted.begin(), sorted.end()))
            << "Sorted output should be sorted";
    });
}

TEST(SortingInvariants, SortPreservesLength) {
    hegel::test([](hegel::TestCase& tc) {
        auto v = tc.draw(
            gs::vectors(gs::integers<int>(), {.min_size = 0, .max_size = 100}));
        size_t original_size = v.size();

        std::sort(v.begin(), v.end());

        ASSERT_EQ(v.size(), original_size)
            << "Sorting should preserve vector length";
    });
}

TEST(SortingInvariants, SortPreservesElements) {
    hegel::test([](hegel::TestCase& tc) {
        auto v = tc.draw(
            gs::vectors(gs::integers<int>(), {.min_size = 0, .max_size = 50}));
        std::multiset<int> original(v.begin(), v.end());

        std::sort(v.begin(), v.end());

        std::multiset<int> sorted(v.begin(), v.end());
        ASSERT_EQ(original, sorted) << "Sorting should preserve all elements";
    });
}

TEST(SortingInvariants, SortIsIdempotent) {
    hegel::test([](hegel::TestCase& tc) {
        auto v = tc.draw(
            gs::vectors(gs::integers<int>(), {.min_size = 0, .max_size = 50}));

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
    hegel::test([](hegel::TestCase& tc) {
        auto s = tc.draw(gs::text({.max_size = 100}));
        tc.note("Testing string of length " + std::to_string(s.size()));

        std::string reversed = s;
        std::reverse(reversed.begin(), reversed.end());
        std::reverse(reversed.begin(), reversed.end());

        ASSERT_EQ(reversed, s) << "Reversing twice should give original";
    });
}

TEST(StringProperties, ConcatenationLengthIsSum) {
    hegel::test([](hegel::TestCase& tc) {
        auto s1 = tc.draw(gs::text({.max_size = 50}));
        auto s2 = tc.draw(gs::text({.max_size = 50}));

        std::string concatenated = s1 + s2;

        ASSERT_EQ(concatenated.size(), s1.size() + s2.size())
            << "Concatenation length should be sum of lengths";
    });
}

TEST(StringProperties, SubstringIsContained) {
    hegel::test([](hegel::TestCase& tc) {
        auto s = tc.draw(gs::text({.min_size = 5, .max_size = 100}));
        tc.assume(s.size() >= 5);

        auto start = tc.draw(
            gs::integers<size_t>({.min_value = 0, .max_value = s.size() - 1}));
        auto len = tc.draw(gs::integers<size_t>(
            {.min_value = 1, .max_value = s.size() - start}));

        std::string sub = s.substr(start, len);

        ASSERT_NE(s.find(sub), std::string::npos)
            << "Substring should be found in original";
    });
}

// =============================================================================
// Collection Properties
// =============================================================================

TEST(CollectionProperties, SetHasNoDuplicates) {
    hegel::test([](hegel::TestCase& tc) {
        auto s = tc.draw(
            gs::sets(gs::integers<int>({.min_value = 0, .max_value = 1000}),
                     {.min_size = 0, .max_size = 50}));

        std::vector<int> elements(s.begin(), s.end());
        std::set<int> unique(elements.begin(), elements.end());

        ASSERT_EQ(elements.size(), unique.size())
            << "Set should have no duplicates";
    });
}

TEST(CollectionProperties, UniqueVectorHasNoDuplicates) {
    hegel::test([](hegel::TestCase& tc) {
        auto v = tc.draw(
            gs::vectors(gs::integers<int>({.min_value = 0, .max_value = 10000}),
                        {.min_size = 0, .max_size = 100, .unique = true}));

        std::set<int> unique(v.begin(), v.end());

        ASSERT_EQ(v.size(), unique.size())
            << "Unique vector should have no duplicates";
    });
}

TEST(CollectionProperties, MapKeysAreUnique) {
    hegel::test([](hegel::TestCase& tc) {
        auto m = tc.draw(gs::maps(gs::text({.min_size = 1, .max_size = 20}),
                                  gs::integers<int>(),
                                  {.min_size = 0, .max_size = 20}));

        std::vector<std::string> keys;
        for (const auto& [k, v] : m) {
            keys.push_back(k);
        }
        std::set<std::string> unique_keys(keys.begin(), keys.end());

        ASSERT_EQ(keys.size(), unique_keys.size())
            << "Map keys should be unique";
    });
}

// =============================================================================
// Numeric Properties
// =============================================================================

TEST(NumericProperties, AbsoluteValueIsNonNegative) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int32_t>());
        // Avoid INT_MIN which has no positive counterpart
        tc.assume(x != std::numeric_limits<int32_t>::min());
        tc.note("Testing abs(" + std::to_string(x) + ")");

        ASSERT_GE(std::abs(x), 0) << "Absolute value should be non-negative";
    });
}

TEST(NumericProperties, MaxIsGreaterOrEqual) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int>());
        auto y = tc.draw(gs::integers<int>());
        tc.note("Testing max(" + std::to_string(x) + ", " + std::to_string(y) +
                ")");

        int m = std::max(x, y);
        ASSERT_GE(m, x) << "Max should be >= first argument";
        ASSERT_GE(m, y) << "Max should be >= second argument";
        ASSERT_TRUE(m == x || m == y) << "Max should equal one of the inputs";
    });
}

TEST(NumericProperties, MinIsLessOrEqual) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int>());
        auto y = tc.draw(gs::integers<int>());

        int m = std::min(x, y);
        ASSERT_LE(m, x) << "Min should be <= first argument";
        ASSERT_LE(m, y) << "Min should be <= second argument";
        ASSERT_TRUE(m == x || m == y) << "Min should equal one of the inputs";
    });
}

TEST(NumericProperties, ClampIsInRange) {
    hegel::test([](hegel::TestCase& tc) {
        auto lo =
            tc.draw(gs::integers<int>({.min_value = -100, .max_value = 0}));
        auto hi =
            tc.draw(gs::integers<int>({.min_value = 0, .max_value = 100}));
        auto x = tc.draw(gs::integers<int>());

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
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int>());
        tc.note("Testing round-trip for " + std::to_string(x));

        std::string s = std::to_string(x);
        int y = std::stoi(s);

        ASSERT_EQ(x, y) << "Int -> string -> int should be identity";
    });
}

TEST(RoundTrip, DoubleToStringToDouble) {
    hegel::test([](hegel::TestCase& tc) {
        // Use bounded floats to avoid special values
        auto x = tc.draw(
            gs::floats<double>({.min_value = -1e10, .max_value = 1e10}));

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

TEST(FilterMapProperties, FilteredValuesMatchPredicate) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen = gs::integers<int>({.min_value = 0, .max_value = 100})
                       .filter([](int x) { return x % 2 == 0; });
        auto x = tc.draw(gen);

        ASSERT_EQ(x % 2, 0) << "Filtered values should be even";
        ASSERT_GE(x, 0) << "Value should be in original range";
        ASSERT_LE(x, 100) << "Value should be in original range";
    });
}

TEST(FilterMapProperties, MappedValuesAreTransformed) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen =
            gs::integers<int>({.min_value = 1, .max_value = 10}).map([](int x) {
                return x * x;
            });
        auto x = tc.draw(gen);

        // Result should be a perfect square between 1 and 100
        int root = static_cast<int>(std::sqrt(x));
        ASSERT_EQ(root * root, x) << "Mapped value should be a perfect square";
        ASSERT_GE(x, 1) << "Value should be >= 1";
        ASSERT_LE(x, 100) << "Value should be <= 100";
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
