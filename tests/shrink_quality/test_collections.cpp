#include <gtest/gtest.h>

#include "common/utils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using hegel::tests::common::minimal;
namespace gs = hegel::generators;

TEST(ShrinkCollections, Minimize3Set) {
    auto v = minimal<std::set<int64_t>>(
        gs::sets(gs::integers<int64_t>()),
        [](const std::set<int64_t>& x) { return x.size() >= 3; });
    bool ok =
        v == std::set<int64_t>{0, 1, 2} || v == std::set<int64_t>{-1, 0, 1};
    EXPECT_TRUE(ok);
}

TEST(ShrinkCollections, MinimizeSetsSampledFrom) {
    std::vector<int64_t> items;
    for (int64_t i = 0; i < 10; ++i)
        items.push_back(i);
    auto v = minimal<std::set<int64_t>>(
        gs::sets(gs::sampled_from(items), {.min_size = 3}),
        [](const std::set<int64_t>&) { return true; });
    EXPECT_EQ(v, (std::set<int64_t>{0, 1, 2}));
}

TEST(ShrinkCollections, Minimize3SetOfTuples) {
    auto gen = gs::sets(gs::tuples(gs::integers<int64_t>()));
    auto v = minimal<std::set<std::tuple<int64_t>>>(
        gen,
        [](const std::set<std::tuple<int64_t>>& x) { return x.size() >= 2; });
    EXPECT_EQ(v, (std::set<std::tuple<int64_t>>{std::tuple<int64_t>{0},
                                                std::tuple<int64_t>{1}}));
}

namespace {
    using VecAndInt = std::pair<std::vector<int64_t>, int64_t>;

    gs::Generator<VecAndInt> vec_and_int_gen() {
        auto v_gen = gs::vectors(gs::integers<int64_t>());
        auto i_gen = gs::integers<int64_t>();
        return gs::compose([v_gen, i_gen](const hegel::TestCase& tc) {
            auto v = v_gen.do_draw(tc);
            int64_t i = i_gen.do_draw(tc);
            return VecAndInt{std::move(v), i};
        });
    }

    void check_containment(int64_t n) {
        auto v = minimal<VecAndInt>(vec_and_int_gen(), [n](const VecAndInt& x) {
            const auto& vec = x.first;
            int64_t i = x.second;
            return i >= n && std::find(vec.begin(), vec.end(), i) != vec.end();
        });
        EXPECT_EQ(v.first, std::vector<int64_t>{n});
        EXPECT_EQ(v.second, n);
    }
} // namespace

TEST(ShrinkCollections, Containment_0) { check_containment(0); }
TEST(ShrinkCollections, Containment_1) { check_containment(1); }
TEST(ShrinkCollections, Containment_10) { check_containment(10); }
TEST(ShrinkCollections, Containment_100) { check_containment(100); }
TEST(ShrinkCollections, Containment_1000) { check_containment(1000); }

TEST(ShrinkCollections, DuplicateContainment) {
    auto v = minimal<VecAndInt>(vec_and_int_gen(), [](const VecAndInt& x) {
        const auto& vec = x.first;
        int64_t i = x.second;
        size_t count =
            static_cast<size_t>(std::count(vec.begin(), vec.end(), i));
        return count > 1;
    });
    EXPECT_EQ(v.first, (std::vector<int64_t>{0, 0}));
    EXPECT_EQ(v.second, 0);
}

TEST(ShrinkCollections, ReorderingBytes) {
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>({.min_value = 0, .max_value = 1000})),
        [](const std::vector<int64_t>& x) {
            int64_t sum = 0;
            for (int64_t e : x)
                sum += e;
            return sum >= 10 && x.size() >= 3;
        });
    auto sorted = v;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(v, sorted);
}

TEST(ShrinkCollections, MinimizeLongList) {
    auto v = minimal<std::vector<bool>>(
        gs::vectors(gs::booleans(), {.min_size = 50}),
        [](const std::vector<bool>& x) { return x.size() >= 70; });
    EXPECT_EQ(v, std::vector<bool>(70, false));
}

TEST(ShrinkCollections, MinimizeListOfLongishLists) {
    const size_t size = 5;
    auto v = minimal<std::vector<std::vector<bool>>>(
        gs::vectors(gs::vectors(gs::booleans())),
        [size](const std::vector<std::vector<bool>>& x) {
            size_t count = 0;
            for (const auto& t : x) {
                bool any_true =
                    std::any_of(t.begin(), t.end(), [](bool b) { return b; });
                if (any_true && t.size() >= 2)
                    ++count;
            }
            return count >= size;
        });
    ASSERT_EQ(v.size(), size);
    std::vector<bool> expected{false, true};
    for (const auto& inner : v) {
        EXPECT_EQ(inner, expected);
    }
}

TEST(ShrinkCollections, MinimizeListOfFairlyNonUniqueInts) {
    auto v = minimal<std::vector<int64_t>>(gs::vectors(gs::integers<int64_t>()),
                                           [](const std::vector<int64_t>& x) {
                                               std::set<int64_t> uniq(x.begin(),
                                                                      x.end());
                                               return uniq.size() < x.size();
                                           });
    EXPECT_EQ(v.size(), 2u);
}

TEST(ShrinkCollections, ListWithComplexSortingStructure) {
    auto v = minimal<std::vector<std::vector<bool>>>(
        gs::vectors(gs::vectors(gs::booleans())),
        [](const std::vector<std::vector<bool>>& x) {
            std::vector<std::vector<bool>> reversed;
            reversed.reserve(x.size());
            for (const auto& inner : x) {
                reversed.emplace_back(inner.rbegin(), inner.rend());
            }
            return reversed > x && x.size() > 3;
        });
    EXPECT_EQ(v.size(), 4u);
}

TEST(ShrinkCollections, ListWithWideGap) {
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>()),
        [](const std::vector<int64_t>& x) {
            if (x.empty())
                return false;
            int64_t maxv = *std::max_element(x.begin(), x.end());
            int64_t minv = *std::min_element(x.begin(), x.end());
            // Guard against overflow when computing min + 10.
            if (minv > std::numeric_limits<int64_t>::max() - 10)
                return false;
            int64_t minv_plus_10 = minv + 10;
            return maxv > minv_plus_10 && minv_plus_10 > 0;
        });
    EXPECT_EQ(v.size(), 2u);
    auto sorted = v;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted[1], sorted[0] + 11);
}

TEST(ShrinkCollections, MinimizeListOfSets) {
    auto v = minimal<std::vector<std::set<bool>>>(
        gs::vectors(gs::sets(gs::booleans())),
        [](const std::vector<std::set<bool>>& x) {
            size_t count = 0;
            for (const auto& s : x) {
                if (!s.empty())
                    ++count;
            }
            return count >= 3;
        });
    std::set<bool> target{false};
    EXPECT_EQ(v, std::vector<std::set<bool>>(3, target));
}

TEST(ShrinkCollections, MinimizeListOfLists) {
    auto v = minimal<std::vector<std::vector<int64_t>>>(
        gs::vectors(gs::vectors(gs::integers<int64_t>())),
        [](const std::vector<std::vector<int64_t>>& x) {
            size_t count = 0;
            for (const auto& s : x) {
                if (!s.empty())
                    ++count;
            }
            return count >= 3;
        });
    std::vector<int64_t> target{0};
    EXPECT_EQ(v, std::vector<std::vector<int64_t>>(3, target));
}

TEST(ShrinkCollections, MinimizeListOfTuples) {
    using Pair = std::tuple<int64_t, int64_t>;
    auto v = minimal<std::vector<Pair>>(
        gs::vectors(
            gs::tuples(gs::integers<int64_t>(), gs::integers<int64_t>())),
        [](const std::vector<Pair>& x) { return x.size() >= 2; });
    EXPECT_EQ(v, (std::vector<Pair>{Pair{0, 0}, Pair{0, 0}}));
}

TEST(ShrinkCollections, MinimizeTupleOfBooleans) {
    using BoolPair = std::tuple<bool, bool>;
    auto pair_gen = gs::tuples(gs::booleans(), gs::booleans());
    auto v = minimal<BoolPair>(pair_gen, [](const BoolPair& x) {
        return std::get<0>(x) || std::get<1>(x);
    });
    // At least one is true; not both.
    EXPECT_TRUE(std::get<0>(v) || std::get<1>(v));
    EXPECT_FALSE(std::get<0>(v) && std::get<1>(v));
}

static void check_forced_near_top(size_t n) {
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>(),
                    {.min_size = n, .max_size = n + 2}),
        [n](const std::vector<int64_t>& t) { return t.size() == n + 2; });
    EXPECT_EQ(v, std::vector<int64_t>(n + 2, 0));
}

TEST(ShrinkCollections, ListsForcedNearTop_0) { check_forced_near_top(0); }
TEST(ShrinkCollections, ListsForcedNearTop_1) { check_forced_near_top(1); }
TEST(ShrinkCollections, ListsForcedNearTop_5) { check_forced_near_top(5); }
TEST(ShrinkCollections, ListsForcedNearTop_10) { check_forced_near_top(10); }

TEST(ShrinkCollections, MapMinimizesToEmpty) {
    auto v = minimal<std::map<int64_t, std::string>>(
        gs::maps(gs::integers<int64_t>(), gs::text()),
        [](const std::map<int64_t, std::string>&) { return true; });
    EXPECT_TRUE(v.empty());
}

TEST(ShrinkCollections, MapMinimizesValues) {
    auto v = minimal<std::map<int64_t, std::string>>(
        gs::maps(gs::integers<int64_t>(), gs::text()),
        [](const std::map<int64_t, std::string>& t) { return t.size() >= 3; });
    EXPECT_GE(v.size(), 3u);
    for (const auto& [k, val] : v) {
        EXPECT_EQ(val, "");
    }
    for (const auto& [k, _] : v) {
        if (k < 0) {
            EXPECT_TRUE(v.contains(k + 1));
        }
        if (k > 0) {
            EXPECT_TRUE(v.contains(k - 1));
        }
    }
}

TEST(ShrinkCollections, MinimizeMultiKeyMaps) {
    auto key_gen = gs::booleans().map(
        [](bool b) -> std::string { return b ? "true" : "false"; });
    auto v = minimal<std::map<std::string, bool>>(
        gs::maps(key_gen, gs::booleans()),
        [](const std::map<std::string, bool>& x) { return !x.empty(); });
    EXPECT_EQ(v.size(), 1u);
    std::map<std::string, bool> expected{{"false", false}};
    EXPECT_EQ(v, expected);
}

TEST(ShrinkCollections, FindLargeUnionList) {
    const size_t size = 10;
    auto v = minimal<std::vector<std::set<int64_t>>>(
        gs::vectors(gs::sets(gs::integers<int64_t>(), {.min_size = 1}),
                    {.min_size = 1}),
        [size](const std::vector<std::set<int64_t>>& x) {
            std::set<int64_t> uni;
            for (const auto& s : x) {
                uni.insert(s.begin(), s.end());
            }
            return uni.size() >= size;
        });
    EXPECT_EQ(v.size(), 1u);
    std::set<int64_t> uni;
    for (const auto& s : v) {
        uni.insert(s.begin(), s.end());
    }
    ASSERT_EQ(uni.size(), size);
    int64_t maxv = *uni.rbegin();
    int64_t minv = *uni.begin();
    EXPECT_EQ(maxv, minv + static_cast<int64_t>(uni.size()) - 1);
}

TEST(ShrinkCollections, FindMap) {
    auto v = minimal<std::map<int64_t, int64_t>>(
        gs::maps(gs::integers<int64_t>(), gs::integers<int64_t>()),
        [](const std::map<int64_t, int64_t>& xs) {
            for (const auto& [k, val] : xs) {
                if (k > val)
                    return true;
            }
            return false;
        });
    EXPECT_EQ(v.size(), 1u);
}

TEST(ShrinkCollections, CanFindList) {
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>()),
        [](const std::vector<int64_t>& x) {
            // Saturating sum to avoid overflow.
            int64_t acc = 0;
            for (int64_t e : x) {
                // check overflow before adding
                if ((e > 0 && acc > std::numeric_limits<int64_t>::max() - e) ||
                    (e < 0 && acc < std::numeric_limits<int64_t>::min() - e))
                    return false;
                acc += e;
            }
            return acc >= 10;
        });
    int64_t sum = 0;
    for (int64_t e : v)
        sum += e;
    EXPECT_EQ(sum, 10);
}

TEST(ShrinkCollections, CollectivelyMinimizeIntegers) {
    const size_t n = 10;
    auto v = minimal<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>(), {.min_size = n, .max_size = n}),
        [](const std::vector<int64_t>& x) {
            std::set<int64_t> uniq(x.begin(), x.end());
            return uniq.size() >= 2;
        },
        2000);
    EXPECT_EQ(v.size(), n);
    std::set<int64_t> uniq(v.begin(), v.end());
    EXPECT_GE(uniq.size(), 2u);
    EXPECT_LE(uniq.size(), 3u);
}

TEST(ShrinkCollections, CollectivelyMinimizeBooleans) {
    const size_t n = 10;
    auto v = minimal<std::vector<bool>>(
        gs::vectors(gs::booleans(), {.min_size = n, .max_size = n}),
        [](const std::vector<bool>& x) {
            std::set<bool> uniq(x.begin(), x.end());
            return uniq.size() >= 2;
        },
        2000);
    EXPECT_EQ(v.size(), n);
    std::set<bool> uniq(v.begin(), v.end());
    EXPECT_EQ(uniq.size(), 2u);
}

TEST(ShrinkCollections, CollectivelyMinimizeText) {
    const size_t n = 10;
    auto v = minimal<std::vector<std::string>>(
        gs::vectors(gs::text(), {.min_size = n, .max_size = n}),
        [](const std::vector<std::string>& x) {
            std::set<std::string> uniq(x.begin(), x.end());
            return uniq.size() >= 2;
        },
        2000);
    EXPECT_EQ(v.size(), n);
    std::set<std::string> uniq(v.begin(), v.end());
    EXPECT_GE(uniq.size(), 2u);
    EXPECT_LE(uniq.size(), 3u);
}
