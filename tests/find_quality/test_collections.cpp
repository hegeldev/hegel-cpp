#include <gtest/gtest.h>

#include "common/utils.h"

#include <cstdint>
#include <set>
#include <vector>

using hegel::tests::common::find_any;
namespace gs = hegel::generators;

TEST(FindCollections, CanProduceLongLists) {
    find_any<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>()),
        [](const std::vector<int64_t>& x) { return x.size() >= 10; });
}

TEST(FindCollections, CanProduceShortLists) {
    find_any<std::vector<int64_t>>(
        gs::vectors(gs::integers<int64_t>()),
        [](const std::vector<int64_t>& x) { return x.size() <= 10; });
}

TEST(FindCollections, CanProduceTheSameIntTwice) {
    find_any<std::vector<int64_t>>(gs::vectors(gs::integers<int64_t>()),
                                   [](const std::vector<int64_t>& x) {
                                       std::set<int64_t> uniq(x.begin(),
                                                              x.end());
                                       return uniq.size() < x.size();
                                   });
}

TEST(FindCollections, SampledFromLargeNumberCanMix) {
    std::vector<int64_t> items;
    for (int64_t i = 0; i < 50; ++i)
        items.push_back(i);
    find_any<std::vector<int64_t>>(
        gs::vectors(gs::sampled_from(items), {.min_size = 50}),
        [](const std::vector<int64_t>& x) {
            std::set<int64_t> uniq(x.begin(), x.end());
            return uniq.size() >= 25;
        });
}

TEST(FindCollections, NonEmptySubsetOfTwoIsUsuallyLarge) {
    find_any<std::set<int64_t>>(
        gs::sets(gs::sampled_from<int64_t>({int64_t{1}, int64_t{2}})),
        [](const std::set<int64_t>& x) { return x.size() == 2; });
}

TEST(FindCollections, SubsetOfTenIsSometimesEmpty) {
    find_any<std::set<int64_t>>(
        gs::sets(gs::integers<int64_t>({.min_value = 1, .max_value = 10})),
        [](const std::set<int64_t>& x) { return x.empty(); });
}
