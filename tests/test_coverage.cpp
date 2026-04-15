// test_coverage.cpp - Tests to exercise uncovered code paths
// Targets format generators, character filtering, binary data, regex,
// JSON wrapper methods, and other gaps found by coverage analysis.

#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;
using hegel::draw;

// =============================================================================
// Format generators
// =============================================================================

TEST(FormatGenerators, Emails) {
    hegel::hegel([]() {
        auto value = draw(emails());
        ASSERT_FALSE(value.empty());
        ASSERT_NE(value.find('@'), std::string::npos);
    });
}

TEST(FormatGenerators, Domains) {
    hegel::hegel([]() {
        auto value = draw(domains());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, DomainsWithMaxLength) {
    hegel::hegel([]() {
        auto value = draw(domains({.max_length = 50}));
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Urls) {
    hegel::hegel([]() {
        auto value = draw(urls());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, IpAddressesV4) {
    hegel::hegel([]() {
        auto value = draw(ip_addresses({.v = 4}));
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, IpAddressesV6) {
    hegel::hegel([]() {
        auto value = draw(ip_addresses({.v = 6}));
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, IpAddressesBoth) {
    hegel::hegel([]() {
        auto value = draw(ip_addresses());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Dates) {
    hegel::hegel([]() {
        auto value = draw(dates());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Times) {
    hegel::hegel([]() {
        auto value = draw(times());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Datetimes) {
    hegel::hegel([]() {
        auto value = draw(datetimes());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, FromRegex) {
    hegel::hegel([]() {
        auto value = draw(from_regex("[A-Z]{2}-[0-9]{4}", true));
        ASSERT_EQ(value.size(), 7u);
    });
}

TEST(FormatGenerators, FromRegexFullmatch) {
    hegel::hegel([]() {
        auto value = draw(from_regex("[a-z]+", true));
        ASSERT_FALSE(value.empty());
    });
}

// =============================================================================
// Primitive generators
// =============================================================================

TEST(PrimitiveGenerators, Nulls) {
    hegel::hegel([]() {
        auto value = draw(nulls());
        (void)value; // std::monostate - just verify it doesn't throw
    });
}

// =============================================================================
// String generators with character filtering
// =============================================================================

TEST(StringGenerators, TextWithCodec) {
    hegel::hegel([]() {
        auto value = draw(text({.codec = "ascii"}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithCodepointRange) {
    hegel::hegel([]() {
        auto value = draw(text({.min_codepoint = 0x41, .max_codepoint = 0x5A}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithCategories) {
    hegel::hegel([]() {
        auto value = draw(text({.categories = std::vector<std::string>{"Lu"}}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithExcludeCategories) {
    hegel::hegel([]() {
        auto value =
            draw(text({.exclude_categories = std::vector<std::string>{"Cc"}}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithIncludeCharacters) {
    hegel::hegel([]() {
        auto value = draw(text({.categories = std::vector<std::string>{"Lu"},
                                .include_characters = "abc"}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithExcludeCharacters) {
    hegel::hegel([]() {
        auto value = draw(text({.exclude_characters = "xyz"}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithAlphabet) {
    hegel::hegel([]() {
        auto value = draw(text({.alphabet = "ACGT"}));
        for (char c : value) {
            ASSERT_TRUE(c == 'A' || c == 'C' || c == 'G' || c == 'T');
        }
    });
}

TEST(StringGenerators, Characters) {
    hegel::hegel([]() {
        auto value =
            draw(characters({.min_codepoint = 0x41, .max_codepoint = 0x5A}));
        ASSERT_EQ(value.size(), 1u);
    });
}

// =============================================================================
// Binary generator
// =============================================================================

TEST(BinaryGenerator, BasicBinary) {
    hegel::hegel([]() {
        auto value = draw(binary());
        (void)value;
    });
}

TEST(BinaryGenerator, BoundedBinary) {
    hegel::hegel([]() {
        auto value = draw(binary({.min_size = 4, .max_size = 16}));
        ASSERT_GE(value.size(), 4u);
        ASSERT_LE(value.size(), 16u);
    });
}

// =============================================================================
// Validation error paths
// =============================================================================

TEST(ValidationErrors, TextAlphabetWithCharParams) {
    ASSERT_THROW(text({.min_codepoint = 0x41, .alphabet = "abc"}),
                 std::invalid_argument);
}

TEST(ValidationErrors, DomainsMaxLengthTooSmall) {
    ASSERT_THROW(domains({.max_length = 3}), std::invalid_argument);
}

TEST(ValidationErrors, DomainsMaxLengthTooLarge) {
    ASSERT_THROW(domains({.max_length = 256}), std::invalid_argument);
}

TEST(ValidationErrors, BinaryMaxSizeLessThanMinSize) {
    ASSERT_THROW(binary({.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(ValidationErrors, IpAddressesInvalidVersion) {
    ASSERT_THROW(ip_addresses({.v = 3}), std::invalid_argument);
}

// =============================================================================
// note() on final replay
// =============================================================================

TEST(NoteOutput, NoteOnFailure) {
    // note() only prints during is_last_run; test that it doesn't
    // throw during normal runs
    hegel::hegel([]() {
        auto x = draw(integers<int>());
        hegel::note("value is " + std::to_string(x));
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
