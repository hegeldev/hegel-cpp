// Numeric generators: integers, floats, booleans — parameter validation and
// IEEE-754 special-value behavior.

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

TEST(Validation, IntegersMinGreaterThanMax) {
    EXPECT_THROW(gs::integers<int>({.min_value = 10, .max_value = 5}),
                 std::invalid_argument);
}

TEST(Validation, IntegersMinEqualMaxDoesNotThrow) {
    EXPECT_NO_THROW(gs::integers<int>({.min_value = 5, .max_value = 5}));
}

TEST(Validation, FloatsAllowNanWithMinValue) {
    EXPECT_THROW(gs::floats({.min_value = 0.0, .allow_nan = true}),
                 std::invalid_argument);
}

TEST(Validation, FloatsAllowNanWithMaxValue) {
    EXPECT_THROW(gs::floats({.max_value = 1.0, .allow_nan = true}),
                 std::invalid_argument);
}

TEST(Validation, FloatsMinGreaterThanMax) {
    EXPECT_THROW(gs::floats({.min_value = 10.0, .max_value = 5.0}),
                 std::invalid_argument);
}

TEST(Validation, FloatsAllowInfinityWithBothBounds) {
    EXPECT_THROW(
        gs::floats(
            {.min_value = 0.0, .max_value = 1.0, .allow_infinity = true}),
        std::invalid_argument);
}

// An unbounded float draw (allow_nan / allow_infinity default to true) must
// actually produce the IEEE-754 special values, not flatten them somewhere
// between the engine and the caller. Observing each special value across the
// run pins this property down.
TEST(Floats, NanAndInfinitySurviveGeneration) {
    bool saw_nan = false;
    bool saw_pos_inf = false;
    bool saw_neg_inf = false;

    auto gen = gs::floats<double>();
    hegel::test(
        [&](hegel::TestCase& tc) {
            double x = tc.draw(gen);
            if (std::isnan(x)) {
                saw_nan = true;
            } else if (std::isinf(x)) {
                (x > 0 ? saw_pos_inf : saw_neg_inf) = true;
            }
        },
        hegel::Settings{.test_cases = 2000,
                        .seed = 1,
                        .database = hegel::Database::disabled()});

    EXPECT_TRUE(saw_nan) << "NaN never generated";
    EXPECT_TRUE(saw_pos_inf) << "+infinity never generated";
    EXPECT_TRUE(saw_neg_inf) << "-infinity never generated";
}
