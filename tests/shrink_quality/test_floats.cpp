#include <gtest/gtest.h>

#include "common/utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

using hegel::tests::common::minimal;
namespace gs = hegel::generators;

TEST(ShrinkFloats, ShrinksToSimpleFloatAbove1) {
    double v =
        minimal<double>(gs::floats<double>(), [](double x) { return x > 1.0; });
    EXPECT_EQ(v, 2.0);
}

TEST(ShrinkFloats, ShrinksToSimpleFloatAbove0) {
    double v =
        minimal<double>(gs::floats<double>(), [](double x) { return x > 0.0; });
    EXPECT_EQ(v, 1.0);
}

static void check_variable_sized_context(size_t n) {
    auto v = minimal<std::vector<double>>(
        gs::vectors(gs::floats<double>(), {.min_size = n}),
        [](const std::vector<double>& xs) {
            return std::any_of(xs.begin(), xs.end(),
                               [](double f) { return f != 0.0; });
        });
    ASSERT_EQ(v.size(), n);
    size_t zeros = static_cast<size_t>(
        std::count_if(v.begin(), v.end(), [](double f) { return f == 0.0; }));
    EXPECT_EQ(zeros, n - 1);
    EXPECT_TRUE(std::find(v.begin(), v.end(), 1.0) != v.end());
}

TEST(ShrinkFloats, CanShrinkInVariableSizedContext_1) {
    check_variable_sized_context(1);
}
TEST(ShrinkFloats, CanShrinkInVariableSizedContext_2) {
    check_variable_sized_context(2);
}
TEST(ShrinkFloats, CanShrinkInVariableSizedContext_3) {
    check_variable_sized_context(3);
}
TEST(ShrinkFloats, CanShrinkInVariableSizedContext_8) {
    check_variable_sized_context(8);
}
TEST(ShrinkFloats, CanShrinkInVariableSizedContext_10) {
    check_variable_sized_context(10);
}

TEST(ShrinkFloats, CanFindNan) {
    double v = minimal<double>(gs::floats<double>(),
                               [](double x) { return std::isnan(x); });
    EXPECT_TRUE(std::isnan(v));
}

TEST(ShrinkFloats, CanFindNans) {
    auto v = minimal<std::vector<double>>(gs::vectors(gs::floats<double>()),
                                          [](const std::vector<double>& xs) {
                                              double sum = 0.0;
                                              for (double x : xs)
                                                  sum += x;
                                              return std::isnan(sum);
                                          });
    if (v.size() == 1) {
        EXPECT_TRUE(std::isnan(v[0]));
    } else {
        EXPECT_GE(v.size(), 2u);
        EXPECT_LE(v.size(), 3u);
    }
}
