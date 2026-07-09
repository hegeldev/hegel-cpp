#include <gtest/gtest.h>

#include <cstdlib>
#include <map>
#include <string>
#include <type_traits>

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

TEST(Validation, TextMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::text({.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, BinaryMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::binary({.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, VectorsMaxSizeLessThanMinSize) {
    EXPECT_THROW(
        gs::vectors(gs::integers<int>(), {.min_size = 10, .max_size = 5}),
        std::invalid_argument);
}

TEST(Validation, SetsMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::sets(gs::integers<int>(), {.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, MapsMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::maps(gs::text(), gs::integers<int>(),
                          {.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, DomainsMaxLengthTooSmall) {
    EXPECT_THROW(gs::domains({.max_length = 3}), std::invalid_argument);
}

TEST(Validation, DomainsMaxLengthTooLarge) {
    EXPECT_THROW(gs::domains({.max_length = 256}), std::invalid_argument);
}

TEST(Validation, IpAddressesInvalidVersion) {
    EXPECT_THROW(gs::ip_addresses({.v = 5}), std::invalid_argument);
}

TEST(Validation, OneOfEmptyVector) {
    EXPECT_THROW(gs::one_of(std::vector<gs::Generator<int>>{}),
                 std::invalid_argument);
}

TEST(JustOverload, StringLiteralDefaultsToString) {
    auto g = gs::just("hello");
    static_assert(
        std::is_same_v<decltype(g), gs::Generator<std::string>>,
        "just(\"hello\") should deduce Generator<std::string> via the "
        "const char* overload");
}

TEST(JustOverload, ExplicitCharPtrKeepsCharPtr) {
    auto g = gs::just<const char*>("hello");
    static_assert(std::is_same_v<decltype(g), gs::Generator<const char*>>,
                  "just<const char*>(\"hello\") should select the template and "
                  "yield Generator<const char*>");
}

namespace {
    // A type with no public fields for reflect-cpp and no default
    // constructor - unambiguously non-serializable. just() and
    // sampled_from() never hand T to the engine (they only draw an index,
    // if anything), so this must still work.
    class OpaqueHandle {
      public:
        explicit OpaqueHandle(int id) : id_(id) {}
        int id() const { return id_; }
        bool operator==(const OpaqueHandle& other) const {
            return id_ == other.id_;
        }

      private:
        int id_;
    };
} // namespace

TEST(NonSerializable, JustWorksWithOpaqueType) {
    auto gen = gs::just(OpaqueHandle{42});
    hegel::test([&gen](hegel::TestCase& tc) {
        OpaqueHandle drawn = tc.draw(gen);
        EXPECT_EQ(drawn.id(), 42);
    });
}

TEST(NonSerializable, SampledFromWorksWithOpaqueType) {
    std::vector<OpaqueHandle> options;
    options.emplace_back(1);
    options.emplace_back(2);
    options.emplace_back(3);
    auto gen = gs::sampled_from(options);
    hegel::test([&gen](hegel::TestCase& tc) {
        OpaqueHandle drawn = tc.draw(gen);
        EXPECT_TRUE(drawn == OpaqueHandle{1} || drawn == OpaqueHandle{2} ||
                    drawn == OpaqueHandle{3});
    });
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

TEST(FailureBlobs, BlobDoesNotReproduceFailure) {
    try {
        hegel::test(
            [&](hegel::TestCase& tc) {
                int n = tc.draw(gs::integers<int>());
                if (n < 50) {
                    throw std::runtime_error("fail");
                }
            },
            {}, {"AAEAAAAACgEAAAAy"});
        FAIL();
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()),
                  "The failure blob did not reproduce an error");
    }
}

TEST(FailureBlobs, InvalidBlob) {
    try {
        hegel::test(
            [&](hegel::TestCase& tc) {
                int n = tc.draw(gs::integers<int>());
                if (n < 50) {
                    throw std::runtime_error("fail");
                }
            },
            {}, {"A"});
        FAIL();
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()),
                  "invalid argument: hegel_test_case_from_blob: the supplied "
                  "failure blob could not be decoded. It may be corrupt or "
                  "from an incompatible Hegel version.");
    }
}

HEGEL_REPRODUCE_FAILURE(obvious_fail, "AAEAAAAACgEAAAAA", "invalid")
HEGEL_TEST(obvious_fail,
           {.phases = {hegel::Phase::Explicit}})(hegel::TestCase& tc) {
    int n = tc.draw(gs::integers<int>());
    if (n < 50) {
        throw std::runtime_error("fail");
    }
}

TEST(FailureBlobs, BlobReproduceFailure) {
    try {
        obvious_fail();
        FAIL();
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "fail");
    }
}

// Every blob given to HEGEL_REPRODUCE_FAILURE is kept in the registry (the
// annotation above registers two), even though hegel::test replays only the
// first. Losing the extras silently would defeat their bookkeeping purpose.
TEST(FailureBlobs, RegistryKeepsAllBlobs) {
    std::vector<std::string> blobs = hegel::internal::reproduce_blobs_for(
        (std::string(__FILE__) + "::obvious_fail").c_str());
    EXPECT_EQ(blobs,
              (std::vector<std::string>{"AAEAAAAACgEAAAAA", "invalid"}));
}

// Registering a second HEGEL_REPRODUCE_FAILURE for the same test must be a
// loud error, not a silent drop of the new blobs.
TEST(FailureBlobs, DuplicateRegistrationThrows) {
    const char* key = "test_hegel.cpp::duplicate_registration_probe";
    EXPECT_TRUE(hegel::internal::register_blob(key, {"blob-one"}));
    EXPECT_THROW(hegel::internal::register_blob(key, {"blob-two"}),
                 std::logic_error);
    // The original registration is untouched by the failed one.
    EXPECT_EQ(hegel::internal::reproduce_blobs_for(key),
              std::vector<std::string>{"blob-one"});
}

TEST(Settings, VerbosityToString) {
    using hegel::Verbosity;
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Quiet), "quiet");
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Verbose), "verbose");
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Debug), "debug");
    EXPECT_STREQ(hegel::verbosity_to_string(Verbosity::Normal), "normal");
}

TEST(Settings, HealthCheckToString) {
    using hegel::HealthCheck;
    EXPECT_STREQ(hegel::health_check_to_string(HealthCheck::FilterTooMuch),
                 "filter_too_much");
    EXPECT_STREQ(hegel::health_check_to_string(HealthCheck::TooSlow),
                 "too_slow");
    EXPECT_STREQ(hegel::health_check_to_string(HealthCheck::TestCasesTooLarge),
                 "test_cases_too_large");
    EXPECT_STREQ(
        hegel::health_check_to_string(HealthCheck::LargeInitialTestCase),
        "large_initial_test_case");
    // An out-of-range value falls through the switch to the empty string.
    EXPECT_STREQ(hegel::health_check_to_string(static_cast<HealthCheck>(999)),
                 "");
}

// in_ci() scans known CI environment variables. Drive it with a controlled
// environment so the result doesn't depend on where the suite runs.
TEST(Settings, InCiDetection) {
    static const char* kCiVars[] = {"CI",
                                    "TF_BUILD",
                                    "BUILDKITE",
                                    "CIRCLECI",
                                    "CIRRUS_CI",
                                    "CODEBUILD_BUILD_ID",
                                    "GITHUB_ACTIONS",
                                    "GITLAB_CI",
                                    "HEROKU_TEST_RUN_ID",
                                    "TEAMCITY_VERSION"};

    // Save and clear the ambient CI variables so the test is deterministic.
    std::map<std::string, std::string> saved;
    for (const char* name : kCiVars) {
        if (const char* v = std::getenv(name)) {
            saved.emplace(name, v);
        }
        unsetenv(name);
    }

    // Nothing set: not in CI.
    EXPECT_FALSE(hegel::internal::in_ci());

    // A presence-only variable (expected == nullptr) satisfies the check.
    setenv("CI", "anything", 1);
    EXPECT_TRUE(hegel::internal::in_ci());
    unsetenv("CI");

    // A variable with an expected value matches only when it is equal.
    setenv("GITHUB_ACTIONS", "true", 1);
    EXPECT_TRUE(hegel::internal::in_ci());
    setenv("GITHUB_ACTIONS", "false", 1);
    EXPECT_FALSE(hegel::internal::in_ci());
    unsetenv("GITHUB_ACTIONS");

    // Restore the original environment.
    for (const char* name : kCiVars) {
        unsetenv(name);
    }
    for (const auto& [name, value] : saved) {
        setenv(name.c_str(), value.c_str(), 1);
    }
}