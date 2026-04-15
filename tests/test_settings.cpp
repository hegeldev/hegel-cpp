#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;

TEST(Settings, DefaultRuns100TestCases) {
    int count = 0;

    hegel::hegel([&count] {
        hegel::draw(integers<int>());
        count++;
    });

    ASSERT_EQ(count, 100);
}

TEST(Settings, SeedMakesRunDeterministic) {
    std::vector<int> first_run, second_run;

    hegel::hegel(
        [&first_run] { first_run.push_back(hegel::draw(integers<int>())); },
        {.seed = 12345});

    hegel::hegel(
        [&second_run] { second_run.push_back(hegel::draw(integers<int>())); },
        {.seed = 12345});

    ASSERT_EQ(first_run, second_run);
}
