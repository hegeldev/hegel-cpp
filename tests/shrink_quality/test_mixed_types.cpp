#include <gtest/gtest.h>

#include "common/utils.h"

#include <cstdint>
#include <set>
#include <string>
#include <variant>
#include <vector>

using hegel::tests::common::minimal;
namespace gs = hegel::generators;

TEST(ShrinkMixed, OneOfIntegers) {
    for (int rep = 0; rep < 10; ++rep) {
        int64_t v = minimal<int64_t>(
            gs::one_of<int64_t>(
                {gs::integers<int64_t>(),
                 gs::integers<int64_t>({.min_value = 100, .max_value = 200})}),
            [](int64_t) { return true; });
        EXPECT_EQ(v, 0);
    }
}

using IntOrTextOrBool = std::variant<int64_t, std::string, bool>;

TEST(ShrinkMixed, OneOfMixed) {
    for (int rep = 0; rep < 10; ++rep) {
        auto v = minimal<IntOrTextOrBool>(
            gs::variant_(gs::integers<int64_t>(), gs::text(), gs::booleans()),
            [](const IntOrTextOrBool&) { return true; });
        bool ok =
            (std::holds_alternative<int64_t>(v) && std::get<int64_t>(v) == 0) ||
            (std::holds_alternative<std::string>(v) &&
             std::get<std::string>(v).empty()) ||
            (std::holds_alternative<bool>(v) && std::get<bool>(v) == false);
        EXPECT_TRUE(ok);
    }
}

using IntOrText = std::variant<int64_t, std::string>;

TEST(ShrinkMixed, MixedList) {
    auto element_gen = gs::variant_(gs::integers<int64_t>(), gs::text());
    auto v = minimal<std::vector<IntOrText>>(
        gs::vectors(element_gen),
        [](const std::vector<IntOrText>& x) { return x.size() >= 10; });
    EXPECT_EQ(v.size(), 10u);
    for (const auto& item : v) {
        bool ok = (std::holds_alternative<int64_t>(item) &&
                   std::get<int64_t>(item) == 0) ||
                  (std::holds_alternative<std::string>(item) &&
                   std::get<std::string>(item).empty());
        EXPECT_TRUE(ok);
    }
}

using BoolOrText = std::variant<bool, std::string>;

TEST(ShrinkMixed, MixedListFlatmap) {
    auto element_gen = gs::booleans().flat_map([](bool b) {
        if (b) {
            return gs::booleans().map(
                [](bool inner) -> BoolOrText { return BoolOrText{inner}; });
        } else {
            return gs::text().map(
                [](std::string s) -> BoolOrText { return BoolOrText{s}; });
        }
    });
    auto v = minimal<std::vector<BoolOrText>>(
        gs::vectors(element_gen), [](const std::vector<BoolOrText>& ls) {
            size_t bools = 0;
            size_t texts = 0;
            for (const auto& e : ls) {
                if (std::holds_alternative<bool>(e))
                    ++bools;
                else
                    ++texts;
            }
            return bools >= 3 && texts >= 3;
        });
    EXPECT_EQ(v.size(), 6u);
    // All bools should be false and all strings should be empty.
    for (const auto& item : v) {
        if (std::holds_alternative<bool>(item)) {
            EXPECT_FALSE(std::get<bool>(item));
        } else {
            EXPECT_TRUE(std::get<std::string>(item).empty());
        }
    }
}

TEST(ShrinkMixed, OneOfSlip) {
    auto v = minimal<int64_t>(
        gs::one_of<int64_t>(
            {gs::integers<int64_t>({.min_value = 101, .max_value = 200}),
             gs::integers<int64_t>({.min_value = 0, .max_value = 100})}),
        [](int64_t) { return true; });
    EXPECT_EQ(v, 101);
}

namespace {
    struct MixedRecord {
        int64_t first;
        std::string filler_text;
        bool filler_bool;
        int64_t filler_int;
        int64_t last;
    };
} // namespace

TEST(ShrinkMixed, BuildsStructShrinksSeparatedFields) {
    auto gen = gs::builds_agg<MixedRecord>(
        gs::field<&MixedRecord::first>(
            gs::integers<int64_t>({.min_value = 0, .max_value = 1000})),
        gs::field<&MixedRecord::filler_text>(gs::text()),
        gs::field<&MixedRecord::filler_bool>(gs::booleans()),
        gs::field<&MixedRecord::filler_int>(gs::integers<int64_t>()),
        gs::field<&MixedRecord::last>(
            gs::integers<int64_t>({.min_value = 0, .max_value = 1000})));
    auto v = minimal<MixedRecord>(
        gen, [](const MixedRecord& r) { return r.first + r.last > 1000; });
    EXPECT_EQ(v.first, 1);
    EXPECT_EQ(v.last, 1000);
}
