#include <gtest/gtest.h>

#include "common/utils.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

using hegel::tests::common::find_any;
using hegel::tests::common::minimal;
namespace gs = hegel::generators;

TEST(FindIntegers, CanProduceZero) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return x == 0; });
}

TEST(FindIntegers, CanProduceLargeMagnitudeIntegers) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return std::abs(x) > 1000; });
}

TEST(FindIntegers, CanProduceLargePositiveIntegers) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return x > 1000; });
}

TEST(FindIntegers, CanProduceLargeNegativeIntegers) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return x < -1000; });
}

TEST(FindIntegers, IntegersAreUsuallyNonZero) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return x != 0; });
}

TEST(FindIntegers, IntegersAreSometimesZero) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return x == 0; });
}

TEST(FindIntegers, IntegersAreOftenSmall) {
    find_any<int64_t>(gs::integers<int64_t>(),
                      [](int64_t x) { return std::abs(x) <= 100; });
}

// TODO XFAIL'd until this issue is fixed:
// https://github.com/HypothesisWorks/hypothesis/issues/4624
//
// once that issue is fixed, remove the try/catch here
TEST(FindIntegers, IntegersAreOftenSmallButNotThatSmall) {
    try {
        find_any<int64_t>(gs::integers<int64_t>(), [](int64_t x) {
            int64_t a = std::abs(x);
            return a >= 50 && a <= 255;
        });
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()).find("find_any: no example matched") ==
            std::string::npos) {
            throw;
        }
    }
}

TEST(FindIntegers, CanOccasionallyBeReallyLarge) {
    find_any<int64_t>(gs::integers<int64_t>(), [](int64_t x) {
        return x >= std::numeric_limits<int64_t>::max() / 2;
    });
}

TEST(FindIntegers, MinimizeNegativeInt) {
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>(),
                               [](int64_t x) { return x < 0; }),
              -1);
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>(),
                               [](int64_t x) { return x < -1; }),
              -2);
}

TEST(FindIntegers, PositiveNegativeInt) {
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>(),
                               [](int64_t x) { return x > 0; }),
              1);
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>(),
                               [](int64_t x) { return x > 1; }),
              2);
}

class FindIntegersDownBoundary : public ::testing::TestWithParam<int64_t> {};

TEST_P(FindIntegersDownBoundary, MinimizeDownToBoundary) {
    int64_t n = GetParam();
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>(),
                               [n](int64_t x) { return x >= n; }),
              n);
}

INSTANTIATE_TEST_SUITE_P(
    , FindIntegersDownBoundary,
    ::testing::Values(int64_t{1}, int64_t{2}, int64_t{3}, int64_t{4},
                      int64_t{7}, int64_t{8}, int64_t{15}, int64_t{16},
                      int64_t{31}, int64_t{32}, int64_t{63}, int64_t{64},
                      int64_t{100}, int64_t{127}, int64_t{128}, int64_t{255},
                      int64_t{256}, int64_t{511}, int64_t{512}, int64_t{1000},
                      int64_t{10000}, int64_t{100000}));

class FindIntegersUpBoundary : public ::testing::TestWithParam<int64_t> {};

TEST_P(FindIntegersUpBoundary, MinimizeUpToBoundary) {
    int64_t n = GetParam();
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>(),
                               [n](int64_t x) { return x <= n; }),
              n);
}

INSTANTIATE_TEST_SUITE_P(, FindIntegersUpBoundary,
                         ::testing::Values(int64_t{-1}, int64_t{-16},
                                           int64_t{-128}, int64_t{-1000}));

TEST(FindIntegers, FromDownToBoundary_16) {
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>({.min_value = 6}),
                               [](int64_t x) { return x >= 16; }),
              16);
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>({.min_value = 16}),
                               [](int64_t) { return true; }),
              16);
}

TEST(FindIntegers, FromDownToBoundary_128) {
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>({.min_value = 118}),
                               [](int64_t x) { return x >= 128; }),
              128);
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>({.min_value = 128}),
                               [](int64_t) { return true; }),
              128);
}

TEST(FindIntegers, FromDownToBoundary_1000) {
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>({.min_value = 990}),
                               [](int64_t x) { return x >= 1000; }),
              1000);
    EXPECT_EQ(minimal<int64_t>(gs::integers<int64_t>({.min_value = 1000}),
                               [](int64_t) { return true; }),
              1000);
}

TEST(FindIntegers, NegativeIntegerRangeUpwards) {
    EXPECT_EQ(minimal<int64_t>(
                  gs::integers<int64_t>({.min_value = -10, .max_value = -1}),
                  [](int64_t) { return true; }),
              -1);
}

TEST(FindIntegers, IntegerRangeToBoundary_16) {
    EXPECT_EQ(minimal<int64_t>(
                  gs::integers<int64_t>({.min_value = 16, .max_value = 116}),
                  [](int64_t) { return true; }),
              16);
}

TEST(FindIntegers, IntegerRangeToBoundary_128) {
    EXPECT_EQ(minimal<int64_t>(
                  gs::integers<int64_t>({.min_value = 128, .max_value = 228}),
                  [](int64_t) { return true; }),
              128);
}

TEST(FindIntegers, SingleIntegerRangeIsRange) {
    EXPECT_EQ(minimal<int64_t>(
                  gs::integers<int64_t>({.min_value = 1, .max_value = 1}),
                  [](int64_t) { return true; }),
              1);
}

TEST(FindIntegers, MinimalSmallNumberInLargeRange) {
    EXPECT_EQ(minimal<int64_t>(
                  gs::integers<int64_t>({.min_value = -(int64_t{1} << 32),
                                         .max_value = (int64_t{1} << 32)}),
                  [](int64_t x) { return x >= 101; }),
              101);
}

TEST(FindIntegers, MinimizesListsOfNegativeIntsUpToBoundary) {
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>(), {.min_size = 10}),
        [](const std::vector<int64_t>& x) {
            size_t c = 0;
            for (int64_t e : x) {
                if (e <= -1)
                    ++c;
            }
            return c >= 10;
        });
    EXPECT_EQ(v, std::vector<int64_t>(10, -1));
}
