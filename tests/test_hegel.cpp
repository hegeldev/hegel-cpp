#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;
using namespace hegel::options;

TEST(OutsideContext, DrawThrowsOutsideTest) {
    try {
        hegel::draw(booleans());
        FAIL() << "Expected exception when calling draw() outside a Hegel "
                  "test";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "draw() cannot be called outside of a Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

TEST(OutsideContext, NoteThrowsOutsideTest) {
    try {
        hegel::note("hello");
        FAIL() << "Expected exception when calling note() outside a Hegel test";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "note() cannot be called outside of a Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

TEST(OutsideContext, AssumeThrowsOutsideTest) {
    try {
        hegel::assume(true);
        FAIL()
            << "Expected exception when calling assume() outside a Hegel test";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "assume() cannot be called outside of a Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

// =============================================================================
// Health check failure: filter_too_much
// =============================================================================

TEST(HealthChecks, FilterTooMuchTriggersFailure) {
    EXPECT_THROW(
        {
            hegel::hegel(
                [] {
                    auto x = hegel::draw(integers<int>());
                    // Reject nearly everything to trigger filter_too_much
                    hegel::assume(x == 42);
                },
                {.test_cases = 10});
        },
        std::runtime_error);
}

// =============================================================================
// Suppressing health checks
// =============================================================================

TEST(HealthChecks, SuppressFilterTooMuchLightFiltering) {
    hegel::hegel(
        [] {
            auto n =
                hegel::draw(integers<int>({.min_value = 0, .max_value = 100}));
            hegel::assume(n < 90);
        },
        {.suppress_health_check = {HealthCheck::FilterTooMuch}});
}

TEST(HealthChecks, SuppressFilterTooMuchHeavyFiltering) {
    hegel::hegel(
        [] {
            auto x = hegel::draw(integers<int>());
            hegel::assume(x == 42);
        },
        {.test_cases = 1,
         .suppress_health_check = {HealthCheck::FilterTooMuch}});
}

TEST(HealthChecks, SuppressMultiple) {
    hegel::hegel(
        [] {
            auto n =
                hegel::draw(integers<int>({.min_value = 0, .max_value = 100}));
            hegel::assume(n < 90);
        },
        {.suppress_health_check = {HealthCheck::FilterTooMuch,
                                   HealthCheck::TooSlow}});
}

TEST(HealthChecks, SuppressAllHealthChecks) {
    hegel::hegel(
        [] {
            auto n =
                hegel::draw(integers<int>({.min_value = 0, .max_value = 100}));
            hegel::assume(n < 90);
        },
        {.suppress_health_check = all_health_checks});
}

TEST(HealthChecks, DataTooLargeSuppressed) {
    hegel::hegel(
        [] {
            auto do_big = hegel::draw(booleans());
            if (do_big) {
                for (int i = 0; i < 100; i++) {
                    hegel::draw(integers<int>());
                }
            }
        },
        {.test_cases = 15,
         .suppress_health_check = {HealthCheck::TestCasesTooLarge,
                                   HealthCheck::TooSlow,
                                   HealthCheck::LargeInitialTestCase}});
}

TEST(HealthChecks, LargeBaseExampleSuppressed) {
    hegel::hegel(
        [] {
            for (int i = 0; i < 10; i++) {
                hegel::draw(
                    vectors(integers<int>(), {.min_size = 50, .max_size = 50}));
            }
        },
        {.test_cases = 15,
         .suppress_health_check = {HealthCheck::LargeInitialTestCase,
                                   HealthCheck::TestCasesTooLarge,
                                   HealthCheck::TooSlow}});
}

static int global_counter = 1;

TEST(FlakyReporting, FlakyGlobalState) {
    global_counter = 1;
    try {
        hegel::hegel(
            [] {
                auto x = hegel::draw(integers<int>());
                if (global_counter == 1) {
                    global_counter--;
                    throw std::runtime_error("first test case fails");
                }
            },
            {.test_cases = 1,
             .suppress_health_check = {
                 hegel::options::HealthCheck::LargeInitialTestCase}});
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Flaky Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

// =============================================================================
// Validation tests
// =============================================================================

TEST(Validation, IntegersMinGreaterThanMax) {
    EXPECT_THROW(integers<int>({.min_value = 10, .max_value = 5}),
                 std::invalid_argument);
}

TEST(Validation, IntegersMinEqualMaxDoesNotThrow) {
    EXPECT_NO_THROW(integers<int>({.min_value = 5, .max_value = 5}));
}

TEST(Validation, FloatsAllowNanWithMinValue) {
    EXPECT_THROW(floats({.min_value = 0.0, .allow_nan = true}),
                 std::invalid_argument);
}

TEST(Validation, FloatsAllowNanWithMaxValue) {
    EXPECT_THROW(floats({.max_value = 1.0, .allow_nan = true}),
                 std::invalid_argument);
}

TEST(Validation, FloatsMinGreaterThanMax) {
    EXPECT_THROW(floats({.min_value = 10.0, .max_value = 5.0}),
                 std::invalid_argument);
}

TEST(Validation, FloatsAllowInfinityWithBothBounds) {
    EXPECT_THROW(
        floats({.min_value = 0.0, .max_value = 1.0, .allow_infinity = true}),
        std::invalid_argument);
}

TEST(Validation, TextMaxSizeLessThanMinSize) {
    EXPECT_THROW(text({.min_size = 10, .max_size = 5}), std::invalid_argument);
}

TEST(Validation, BinaryMaxSizeLessThanMinSize) {
    EXPECT_THROW(binary({.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, VectorsMaxSizeLessThanMinSize) {
    EXPECT_THROW(vectors(integers<int>(), {.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, SetsMaxSizeLessThanMinSize) {
    EXPECT_THROW(sets(integers<int>(), {.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, DictionariesMaxSizeLessThanMinSize) {
    EXPECT_THROW(
        dictionaries(text(), integers<int>(), {.min_size = 10, .max_size = 5}),
        std::invalid_argument);
}

TEST(Validation, DomainsMaxLengthTooSmall) {
    EXPECT_THROW(domains({.max_length = 3}), std::invalid_argument);
}

TEST(Validation, DomainsMaxLengthTooLarge) {
    EXPECT_THROW(domains({.max_length = 256}), std::invalid_argument);
}

TEST(Validation, IpAddressesInvalidVersion) {
    EXPECT_THROW(ip_addresses({.v = 5}), std::invalid_argument);
}

TEST(Validation, OneOfEmptyVector) {
    EXPECT_THROW(one_of(std::vector<Generator<int>>{}), std::invalid_argument);
}
