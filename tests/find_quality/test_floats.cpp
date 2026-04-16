#include <gtest/gtest.h>

#include "common/utils.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>

using hegel::tests::common::find_any;
using hegel::tests::common::minimal;
namespace gs = hegel::generators;

static constexpr double kInf = std::numeric_limits<double>::infinity();
static constexpr double kMaxD = std::numeric_limits<double>::max();

TEST(FindFloats, PositiveInfinity) {
    find_any<double>(gs::floats<double>(), [](double x) { return x == kInf; });
}

TEST(FindFloats, NegativeInfinity) {
    find_any<double>(gs::floats<double>(), [](double x) { return x == -kInf; });
}

TEST(FindFloats, Nan) {
    find_any<double>(gs::floats<double>(),
                     [](double x) { return std::isnan(x); });
}

TEST(FindFloats, NearLeft) {
    find_any<double>(gs::floats<double>({.min_value = 0.0, .max_value = 1.0}),
                     [](double t) { return t < 0.2; });
}

TEST(FindFloats, NearRight) {
    find_any<double>(gs::floats<double>({.min_value = 0.0, .max_value = 1.0}),
                     [](double t) { return t > 0.8; });
}

TEST(FindFloats, InMiddle) {
    find_any<double>(gs::floats<double>({.min_value = 0.0, .max_value = 1.0}),
                     [](double t) { return t >= 0.2 && t <= 0.8; });
}

TEST(FindFloats, MostlySensible) {
    find_any<double>(gs::floats<double>(),
                     [](double t) { return t + 1.0 > t; });
}

TEST(FindFloats, MostlyLargish) {
    find_any<double>(gs::floats<double>(),
                     [](double x) { return x > 0.0 && x + 1.0 > 1.0; });
}

TEST(FindFloats, InversionIsImperfect) {
    find_any<double>(gs::floats<double>(), [](double x) {
        return x != 0.0 && !std::isnan(x) && !std::isinf(x) &&
               x * (1.0 / x) != 1.0;
    });
}

TEST(FindFloats, DoNotRoundTripThroughStrings) {
    find_any<double>(gs::floats<double>(), [](double x) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", x);
        char* end = nullptr;
        double y = std::strtod(buf, &end);
        if (end == buf || *end != '\0')
            return true;
        return x != y || (std::isnan(x) != std::isnan(y));
    });
}

TEST(FindFloats, MinimalFloatIsZero) {
    EXPECT_EQ(
        minimal<double>(gs::floats<double>(), [](double) { return true; }),
        0.0);
}

TEST(FindFloats, MinimalsBoundaryFloats) {
    EXPECT_EQ(minimal<double>(
                  gs::floats<double>({.min_value = -1.0, .max_value = 1.0}),
                  [](double) { return true; }),
              0.0);
}

TEST(FindFloats, MinimalNonBoundaryFloat) {
    double x = minimal<double>(
        gs::floats<double>({.min_value = 1.0, .max_value = 9.0}),
        [](double v) { return v > 2.0; });
    EXPECT_EQ(x, 3.0);
}

TEST(FindFloats, MinimalAsymmetricBoundedFloat) {
    double x = minimal<double>(
        gs::floats<double>({.min_value = 1.1, .max_value = 1.6}),
        [](double) { return true; });
    EXPECT_EQ(x, 1.5);
}

TEST(FindFloats, NegativeFloatsSimplifyToZero) {
    double x = minimal<double>(gs::floats<double>(),
                               [](double v) { return v <= -1.0; });
    EXPECT_EQ(x, -1.0);
}

TEST(FindFloats, MinimalInfiniteFloatIsPositive) {
    double x = minimal<double>(gs::floats<double>(),
                               [](double v) { return std::isinf(v); });
    EXPECT_EQ(x, kInf);
}

TEST(FindFloats, CanMinimalInfiniteNegativeFloat) {
    double x = minimal<double>(gs::floats<double>(),
                               [](double v) { return v < -kMaxD; });
    EXPECT_TRUE(std::isinf(x) && x < 0.0);
}

TEST(FindFloats, MinimalFloatOnBoundaryOfRepresentable) {
    double x = minimal<double>(gs::floats<double>(), [](double v) {
        return v + 1.0 == v && !std::isinf(v);
    });
    EXPECT_TRUE(std::isfinite(x));
    EXPECT_EQ(x + 1.0, x);
}

TEST(FindFloats, MinimizeNan) {
    double x = minimal<double>(gs::floats<double>(),
                               [](double v) { return std::isnan(v); });
    EXPECT_TRUE(std::isnan(x));
}

TEST(FindFloats, MinimizeVeryLargeFloat) {
    double t = kMaxD / 2.0;
    double x =
        minimal<double>(gs::floats<double>(), [t](double v) { return v >= t; });
    EXPECT_EQ(x, t);
}

TEST(FindFloats, MinimalFractionalFloat) {
    double x = minimal<double>(gs::floats<double>(),
                               [](double v) { return v >= 1.5; });
    EXPECT_EQ(x, 2.0);
}

TEST(FindFloats, MinimalSmallSumFloatList) {
    auto v = minimal<std::vector<double>>(
        gs::vectors(gs::floats<double>(), {.min_size = 5}),
        [](const std::vector<double>& x) {
            double s = 0.0;
            for (double e : x)
                s += e;
            return s >= 1.0;
        });
    std::vector<double> expected{0.0, 0.0, 0.0, 0.0, 1.0};
    EXPECT_EQ(v, expected);
}

TEST(FindFloats, ListOfFractionalFloat) {
    auto v = minimal<std::vector<double>>(
        gs::vectors(gs::floats<double>(), {.min_size = 5}),
        [](const std::vector<double>& x) {
            size_t c = 0;
            for (double t : x) {
                if (t >= 1.5)
                    ++c;
            }
            return c >= 5;
        });
    std::set<uint64_t> unique;
    for (double f : v) {
        uint64_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        unique.insert(bits);
    }
    uint64_t two_bits;
    double two = 2.0;
    std::memcpy(&two_bits, &two, sizeof(two_bits));
    std::set<uint64_t> expected{two_bits};
    EXPECT_EQ(unique, expected);
}

TEST(FindFloats, BoundsAreRespected) {
    EXPECT_EQ(minimal<double>(gs::floats<double>({.min_value = 1.0}),
                              [](double) { return true; }),
              1.0);
    EXPECT_EQ(minimal<double>(gs::floats<double>({.max_value = -1.0}),
                              [](double) { return true; }),
              -1.0);
}

TEST(FindFloats, FromZeroHaveReasonableRange_0) {
    EXPECT_EQ(minimal<double>(gs::floats<double>({.min_value = 0.0}),
                              [](double x) { return x >= 1.0; }),
              1.0);
}

TEST(FindFloats, FromZeroHaveReasonableRange_3) {
    EXPECT_EQ(minimal<double>(gs::floats<double>({.min_value = 0.0}),
                              [](double x) { return x >= 1000.0; }),
              1000.0);
}

TEST(FindFloats, FromZeroHaveReasonableRange_6) {
    EXPECT_EQ(minimal<double>(gs::floats<double>({.min_value = 0.0}),
                              [](double x) { return x >= 1000000.0; }),
              1000000.0);
}

TEST(FindFloats, ExplicitAllowNan) {
    double x = minimal<double>(gs::floats<double>(),
                               [](double v) { return std::isnan(v); });
    EXPECT_TRUE(std::isnan(x));
}

TEST(FindFloats, OneSidedContainsInfinity) {
    double x = minimal<double>(gs::floats<double>({.min_value = 1.0}),
                               [](double v) { return std::isinf(v); });
    EXPECT_TRUE(std::isinf(x));
}

TEST(FindFloats, CanMinimalFloatFarFromIntegral) {
    double x = minimal<double>(gs::floats<double>(), [](double v) {
        if (!std::isfinite(v))
            return false;
        double scaled = v * static_cast<double>(uint64_t{1} << 32);
        double frac = scaled - std::trunc(scaled);
        return frac != 0.0;
    });
    EXPECT_TRUE(std::isfinite(x));
}
