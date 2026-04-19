#include <gtest/gtest.h>

#include <utils.h>

using namespace hegel::impl::utils;

TEST(Utils, WhichFindsKnownBinary) { EXPECT_TRUE(which("sh").has_value()); }

TEST(Utils, WhichReturnsNoneForMissing) {
    EXPECT_FALSE(which("definitely_not_a_real_binary_xyz").has_value());
}
