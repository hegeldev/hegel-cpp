#include <gtest/gtest.h>

#include "common/utils.h"

#include <cstdint>
#include <limits>
#include <vector>

using hegel::tests::common::minimal;
namespace gs = hegel::generators;

TEST(ShrinkIntegers, FromMinimizesLeftwards) {
    int64_t v = minimal<int64_t>(gs::integers<int64_t>({.min_value = 101}),
                                 [](int64_t) { return true; });
    EXPECT_EQ(v, 101);
}

TEST(ShrinkIntegers, MinimizeBoundedIntegersToZero) {
    int64_t v = minimal<int64_t>(
        gs::integers<int64_t>({.min_value = -10, .max_value = 10}),
        [](int64_t) { return true; });
    EXPECT_EQ(v, 0);
}

TEST(ShrinkIntegers, MinimizeBoundedIntegersToPositive) {
    int64_t v = minimal<int64_t>(
        gs::integers<int64_t>({.min_value = -10, .max_value = 10})
            .filter([](const int64_t& x) { return x != 0; }),
        [](int64_t) { return true; });
    EXPECT_EQ(v, 1);
}

TEST(ShrinkIntegers, SingleElementSillyLargeRange) {
    int64_t v = minimal<int64_t>(
        gs::integers<int64_t>(
            {.min_value = std::numeric_limits<int64_t>::min() / 2,
             .max_value = std::numeric_limits<int64_t>::max() / 2}),
        [](const int64_t& x) {
            return x >= std::numeric_limits<int64_t>::min() / 4;
        });
    EXPECT_EQ(v, 0);
}

TEST(ShrinkIntegers, MultipleElementsSillyLargeRange) {
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>(
            {.min_value = std::numeric_limits<int64_t>::min() / 2,
             .max_value = std::numeric_limits<int64_t>::max() / 2})),
        [](const std::vector<int64_t>& x) { return x.size() >= 20; }, 10000);
    EXPECT_EQ(v, std::vector<int64_t>(20, 0));
}

TEST(ShrinkIntegers, MultipleElementsMinIsNotDupe) {
    auto gen = gs::vectors(gs::integers<int64_t>(
        {.min_value = 0,
         .max_value = std::numeric_limits<int64_t>::max() / 2}));
    std::vector<int64_t> target;
    target.reserve(20);
    for (int64_t i = 0; i < 20; ++i)
        target.push_back(i);

    auto v = minimal<std::vector<int64_t>>(
        gen,
        [target](const std::vector<int64_t>& x) {
            if (x.size() < 20)
                return false;
            for (size_t i = 0; i < 20; ++i) {
                if (x[i] < target[i])
                    return false;
            }
            return true;
        },
        10000);
    EXPECT_EQ(v, target);
}

TEST(ShrinkIntegers, CanFindAnInt) {
    int64_t v =
        minimal<int64_t>(gs::integers<int64_t>(), [](int64_t) { return true; });
    EXPECT_EQ(v, 0);
}

TEST(ShrinkIntegers, CanFindAnIntAbove13) {
    int64_t v = minimal<int64_t>(gs::integers<int64_t>(),
                                 [](int64_t x) { return x >= 13; });
    EXPECT_EQ(v, 13);
}
