#include <gtest/gtest.h>

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
    // A type with no to_json/from_json, no public fields for reflect-cpp,
    // and no default constructor - unambiguously non-serializable. just()
    // and sampled_from() never send T over the wire, so this must still
    // work.
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
