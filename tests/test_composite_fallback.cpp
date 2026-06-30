#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

namespace {
    gs::Generator<int> always_even() {
        return gs::integers<int>({.min_value = 0, .max_value = 100})
            .filter([](const int& x) { return x % 2 == 0; });
    }

    hegel::Settings fast() {
        // The even-only filter rejects ~half its draws; across a composite of
        // several elements that adds up, so suppress the health checks.
        return hegel::Settings{.test_cases = 50,
                               .database = hegel::Database::disabled(),
                               .suppress_health_check =
                                   hegel::all_health_checks()};
    }
} // namespace

TEST(CompositeFallback, SetsWithFunctionBackedElement) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto s = tc.draw(
                gs::sets(always_even(), {.min_size = 1, .max_size = 5}));
            EXPECT_GE(s.size(), 1u);
            EXPECT_LE(s.size(), 5u);
            for (int x : s) {
                EXPECT_EQ(x % 2, 0) << "set element should be even";
            }
        },
        fast());
}

TEST(CompositeFallback, MapsWithFunctionBackedKey) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto m = tc.draw(gs::maps(always_even(), gs::integers<int>(),
                                      {.min_size = 1, .max_size = 5}));
            EXPECT_GE(m.size(), 1u);
            EXPECT_LE(m.size(), 5u);
            for (const auto& [k, v] : m) {
                EXPECT_EQ(k % 2, 0) << "map key should be even";
            }
        },
        fast());
}

TEST(CompositeFallback, TuplesWithFunctionBackedElement) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto t = tc.draw(gs::tuples(always_even(), gs::booleans()));
            EXPECT_GE(std::get<0>(t), 0);
            EXPECT_LE(std::get<0>(t), 100);
            EXPECT_EQ(std::get<0>(t) % 2, 0) << "tuple element should be even";
        },
        fast());
}

TEST(CompositeFallback, VariantWithFunctionBackedBranch) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto v = tc.draw(gs::variant(always_even(), gs::booleans()));
            EXPECT_LT(v.index(), 2u);
            if (v.index() == 0) {
                EXPECT_EQ(std::get<0>(v) % 2, 0)
                    << "variant int should be even";
            }
        },
        fast());
}

TEST(CompositeFallback, SampledFromEmptyThrows) {
    EXPECT_THROW(gs::sampled_from(std::vector<int>{}), std::invalid_argument);
}

TEST(CompositeFallback, SampledFromStringLiterals) {
    hegel::test(
        [](hegel::TestCase& tc) {
            // The initializer_list<const char*> overload yields std::string.
            std::string s = tc.draw(gs::sampled_from({"red", "green", "blue"}));
            EXPECT_TRUE(s == "red" || s == "green" || s == "blue");
        },
        fast());
}

TEST(CompositeFallback, UnsatisfiableUniqueMapIsRejected) {
    hegel::test(
        [](hegel::TestCase& tc) {
            // Only two possible keys (0, 1) but at least five required.
            (void)tc.draw(
                gs::maps(gs::integers<int>({.min_value = 0, .max_value = 1}),
                         gs::integers<int>(), {.min_size = 5}));
        },
        hegel::Settings{.test_cases = 10,
                        .database = hegel::Database::disabled(),
                        .suppress_health_check = hegel::all_health_checks()});
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
