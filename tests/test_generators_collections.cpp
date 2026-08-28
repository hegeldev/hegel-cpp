// Collection generators: vectors, sets, maps, and fixed-size arrays — drawing
// and parameter validation.

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

// arrays<T, N>() draws exactly N elements, each from the given generator.
TEST(Arrays, DrawsExactlyNInRange) {
    hegel::test(
        [](hegel::TestCase& tc) {
            std::array<int, 4> a = tc.draw(gs::arrays<int, 4>(
                gs::integers<int>({.min_value = 0, .max_value = 9})));
            static_assert(std::tuple_size_v<decltype(a)> == 4);
            for (int x : a) {
                EXPECT_GE(x, 0);
                EXPECT_LE(x, 9);
            }
        },
        hegel::Settings{.test_cases = 100,
                        .derandomize = true,
                        .database = hegel::Database::disabled()});
}

// A fixed-size array generator composes inside other generators.
TEST(Arrays, NestsInsideOtherGenerators) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto rows = tc.draw(gs::vectors(
                gs::arrays<int, 2>(gs::integers<int>()), {.max_size = 3}));
            EXPECT_LE(rows.size(), 3u);
            for (const std::array<int, 2>& row : rows) {
                EXPECT_EQ(row.size(), 2u);
            }
        },
        hegel::Settings{.test_cases = 100,
                        .derandomize = true,
                        .database = hegel::Database::disabled()});
}

TEST(Validation, VectorsMaxSizeLessThanMinSize) {
    EXPECT_THROW(
        gs::vectors(gs::integers<int>(), {.min_size = 10, .max_size = 5}),
        std::invalid_argument);
}

TEST(Validation, SetsMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::sets(gs::integers<int>(), {.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, MapsMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::maps(gs::text(), gs::integers<int>(),
                          {.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}
