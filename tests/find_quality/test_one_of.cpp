#include <gtest/gtest.h>

#include "common/utils.h"

#include <cstdint>
#include <vector>

using hegel::tests::common::find_any;
namespace gs = hegel::generators;

namespace {
    gs::Generator<int64_t> nested_one_of() {
        return gs::one_of<int64_t>(
            {gs::just<int64_t>(0),
             gs::one_of<int64_t>(
                 {gs::just<int64_t>(1), gs::just<int64_t>(2),
                  gs::one_of<int64_t>(
                      {gs::just<int64_t>(3), gs::just<int64_t>(4),
                       gs::one_of<int64_t>({gs::just<int64_t>(5),
                                            gs::just<int64_t>(6),
                                            gs::just<int64_t>(7)})})})});
    }
} // namespace

class FindOneOfFlat : public ::testing::TestWithParam<int64_t> {};

TEST_P(FindOneOfFlat, Reaches) {
    int64_t target = GetParam();
    find_any<int64_t>(nested_one_of(),
                      [target](int64_t x) { return x == target; });
}

INSTANTIATE_TEST_SUITE_P(, FindOneOfFlat,
                         ::testing::Values(int64_t{0}, int64_t{1}, int64_t{2},
                                           int64_t{3}, int64_t{4}, int64_t{5},
                                           int64_t{6}, int64_t{7}));

namespace {
    gs::Generator<int64_t> nested_one_of_with_map() {
        return gs::one_of<int64_t>(
            {gs::just<int64_t>(1),
             gs::one_of<int64_t>(
                 {gs::one_of<int64_t>(
                      {gs::just<int64_t>(2), gs::just<int64_t>(3)})
                      .map([](int64_t x) { return x * 2; }),
                  gs::one_of<int64_t>(
                      {gs::one_of<int64_t>(
                           {gs::just<int64_t>(4), gs::just<int64_t>(5)})
                           .map([](int64_t x) { return x * 2; }),
                       gs::one_of<int64_t>({gs::just<int64_t>(6),
                                            gs::just<int64_t>(7),
                                            gs::just<int64_t>(8)})
                           .map([](int64_t x) { return x * 2; })})
                      .map([](int64_t x) { return x * 2; })})});
    }
} // namespace

class FindOneOfMap : public ::testing::TestWithParam<int64_t> {};

TEST_P(FindOneOfMap, Reaches) {
    int64_t target = GetParam();
    find_any<int64_t>(nested_one_of_with_map(),
                      [target](int64_t x) { return x == target; });
}

INSTANTIATE_TEST_SUITE_P(, FindOneOfMap,
                         ::testing::Values(int64_t{1}, int64_t{4}, int64_t{6},
                                           int64_t{16}, int64_t{20},
                                           int64_t{24}, int64_t{28},
                                           int64_t{32}));

namespace {
    // We use std::vector<int> as an analog for Vec<()> — the specific unit
    // type doesn't matter, only the size.
    gs::Generator<std::vector<int>> nested_one_of_with_flatmap() {
        return gs::just<int>(0).flat_map([](int) {
            return gs::one_of<std::vector<int>>(
                {gs::just<std::vector<int>>(std::vector<int>(0)),
                 gs::just<std::vector<int>>(std::vector<int>(1)),
                 gs::one_of<std::vector<int>>(
                     {gs::just<std::vector<int>>(std::vector<int>(2)),
                      gs::just<std::vector<int>>(std::vector<int>(3)),
                      gs::one_of<std::vector<int>>(
                          {gs::just<std::vector<int>>(std::vector<int>(4)),
                           gs::just<std::vector<int>>(std::vector<int>(5)),
                           gs::one_of<std::vector<int>>(
                               {gs::just<std::vector<int>>(std::vector<int>(6)),
                                gs::just<std::vector<int>>(
                                    std::vector<int>(7))})})})});
        });
    }
} // namespace

class FindOneOfFlatmap : public ::testing::TestWithParam<size_t> {};

TEST_P(FindOneOfFlatmap, Reaches) {
    size_t target = GetParam();
    find_any<std::vector<int>>(
        nested_one_of_with_flatmap(),
        [target](const std::vector<int>& v) { return v.size() == target; });
}

INSTANTIATE_TEST_SUITE_P(, FindOneOfFlatmap,
                         ::testing::Values(size_t{0}, size_t{1}, size_t{2},
                                           size_t{3}, size_t{4}, size_t{5},
                                           size_t{6}, size_t{7}));

namespace {
    gs::Generator<int64_t> nested_one_of_with_filter() {
        return gs::one_of<int64_t>(
                   {gs::just<int64_t>(0), gs::just<int64_t>(1),
                    gs::one_of<int64_t>(
                        {gs::just<int64_t>(2), gs::just<int64_t>(3),
                         gs::one_of<int64_t>(
                             {gs::just<int64_t>(4), gs::just<int64_t>(5),
                              gs::one_of<int64_t>({gs::just<int64_t>(6),
                                                   gs::just<int64_t>(7)})})})})
            .filter([](const int64_t& x) { return x % 2 == 0; });
    }
} // namespace

class FindOneOfFilter : public ::testing::TestWithParam<int64_t> {};

TEST_P(FindOneOfFilter, Reaches) {
    int64_t target = GetParam();
    find_any<int64_t>(nested_one_of_with_filter(),
                      [target](int64_t x) { return x == target; });
}

INSTANTIATE_TEST_SUITE_P(, FindOneOfFilter,
                         ::testing::Values(int64_t{0}, int64_t{2}, int64_t{4},
                                           int64_t{6}));
