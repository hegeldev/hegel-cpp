#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;

TEST(Random, RandomsGenerate) {
    hegel::hegel([] {
        auto rng = hegel::draw(randoms());
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        uint64_t v = dist(rng);
        EXPECT_TRUE(v >= 0 && v <= UINT64_MAX);
    });
}

TEST(Random, TrueRandomsGenerate) {
    hegel::hegel([] {
        auto rng = hegel::draw(randoms({.use_true_random = true}));
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        uint64_t v = dist(rng);
        EXPECT_TRUE(v >= 0 && v <= UINT64_MAX);
    });
}

TEST(Random, TestRejectionSamplingDist) {
    hegel::hegel([] {
        auto rng = hegel::draw(randoms({.use_true_random = true}));
        std::normal_distribution<double> dist(0.0, 1.0);
        EXPECT_NO_THROW(dist(rng));
    });
}
