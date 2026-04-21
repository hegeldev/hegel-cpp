#include <gtest/gtest.h>

#include "common/utils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using hegel::tests::common::minimal;
namespace gs = hegel::generators;

TEST(ShrinkFlatmap, BoundedLeftHandSize) {
    auto gen = gs::booleans().flat_map(
        [](bool x) { return gs::vectors(gs::just<bool>(x)); });
    auto v = minimal<std::vector<bool>>(
        gen, [](const std::vector<bool>& x) { return x.size() >= 10; });
    EXPECT_EQ(v, std::vector<bool>(10, false));
}

TEST(ShrinkFlatmap, AcrossFlatmapOfJust) {
    auto gen = gs::integers<int64_t>().flat_map(
        [](int64_t x) { return gs::just<int64_t>(x); });
    int64_t v = minimal<int64_t>(gen, [](int64_t) { return true; });
    EXPECT_EQ(v, 0);
}

TEST(ShrinkFlatmap, RightHandStrategyOfFlatmap) {
    auto gen = gs::integers<int64_t>().flat_map(
        [](int64_t x) { return gs::vectors(gs::just<int64_t>(x)); });
    auto v =
        minimal<std::vector<int64_t>>(gen, [](const auto&) { return true; });
    EXPECT_TRUE(v.empty());
}

TEST(ShrinkFlatmap, IgnoreLeftHandSide) {
    auto gen = gs::integers<int64_t>().flat_map(
        [](int64_t) { return gs::vectors(gs::integers<int64_t>()); });
    auto v = minimal<std::vector<int64_t>>(
        gen, [](const std::vector<int64_t>& x) { return x.size() >= 10; });
    EXPECT_EQ(v, std::vector<int64_t>(10, 0));
}

TEST(ShrinkFlatmap, BothSidesOfFlatmap) {
    auto gen = gs::integers<int64_t>().flat_map(
        [](int64_t x) { return gs::vectors(gs::just<int64_t>(x)); });
    auto v = minimal<std::vector<int64_t>>(
        gen, [](const std::vector<int64_t>& x) { return x.size() >= 10; });
    EXPECT_EQ(v, std::vector<int64_t>(10, 0));
}

TEST(ShrinkFlatmap, Rectangles) {
    auto gen = gs::integers<size_t>({.min_value = 0, .max_value = 10})
                   .flat_map([](size_t w) {
                       return gs::vectors(gs::vectors(
                           gs::sampled_from<std::string>({"a", "b"}),
                           {.min_size = w, .max_size = w}));
                   });
    auto v = minimal<std::vector<std::vector<std::string>>>(
        gen,
        [](const std::vector<std::vector<std::string>>& x) {
            std::vector<std::string> target{"a", "b"};
            return std::find(x.begin(), x.end(), target) != x.end();
        },
        2000);
    EXPECT_EQ(v.size(), 1u);
    ASSERT_FALSE(v.empty());
    std::vector<std::string> expected{"a", "b"};
    EXPECT_EQ(v[0], expected);
}

static void check_shrink_through_binding(size_t n) {
    auto gen = gs::integers<size_t>({.min_value = 0, .max_value = 100})
                   .flat_map([](size_t k) {
                       return gs::vectors(gs::booleans(),
                                          {.min_size = k, .max_size = k});
                   });
    auto v = minimal<std::vector<bool>>(gen, [n](const std::vector<bool>& x) {
        size_t trues =
            static_cast<size_t>(std::count(x.begin(), x.end(), true));
        return trues >= n;
    });
    EXPECT_EQ(v, std::vector<bool>(n, true));
}

TEST(ShrinkFlatmap, ShrinkThroughBinding_1) { check_shrink_through_binding(1); }
TEST(ShrinkFlatmap, ShrinkThroughBinding_3) { check_shrink_through_binding(3); }
TEST(ShrinkFlatmap, ShrinkThroughBinding_5) { check_shrink_through_binding(5); }
TEST(ShrinkFlatmap, ShrinkThroughBinding_9) { check_shrink_through_binding(9); }

static void check_delete_in_middle(size_t n) {
    auto gen = gs::integers<size_t>({.min_value = 1, .max_value = 100})
                   .flat_map([](size_t k) {
                       return gs::vectors(gs::booleans(),
                                          {.min_size = k, .max_size = k});
                   });
    auto v = minimal<std::vector<bool>>(gen, [n](const std::vector<bool>& x) {
        if (x.size() < 2)
            return false;
        if (!x.front())
            return false;
        if (!x.back())
            return false;
        size_t falses =
            static_cast<size_t>(std::count(x.begin(), x.end(), false));
        return falses >= n;
    });
    std::vector<bool> expected;
    expected.reserve(n + 2);
    expected.push_back(true);
    for (size_t i = 0; i < n; ++i)
        expected.push_back(false);
    expected.push_back(true);
    EXPECT_EQ(v, expected);
}

TEST(ShrinkFlatmap, DeleteInMiddle_1) { check_delete_in_middle(1); }
TEST(ShrinkFlatmap, DeleteInMiddle_3) { check_delete_in_middle(3); }
TEST(ShrinkFlatmap, DeleteInMiddle_5) { check_delete_in_middle(5); }
TEST(ShrinkFlatmap, DeleteInMiddle_9) { check_delete_in_middle(9); }
