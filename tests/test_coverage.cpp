// test_coverage.cpp - Tests to exercise uncovered code paths
// Targets format generators, character filtering, binary data, regex,
// and validation paths not covered by other test binaries.

#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;

// =============================================================================
// Format generators
// =============================================================================

TEST(FormatGenerators, Emails) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(emails());
        ASSERT_FALSE(value.empty());
        ASSERT_NE(value.find('@'), std::string::npos);
    });
}

TEST(FormatGenerators, Domains) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(domains());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, DomainsWithMaxLength) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(domains({.max_length = 50}));
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Urls) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(urls());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, IpAddressesV4) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(ip_addresses({.v = 4}));
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, IpAddressesV6) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(ip_addresses({.v = 6}));
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, IpAddressesBoth) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(ip_addresses());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Dates) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(dates());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Times) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(times());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, Datetimes) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(datetimes());
        ASSERT_FALSE(value.empty());
    });
}

TEST(FormatGenerators, FromRegex) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(from_regex("[A-Z]{2}-[0-9]{4}", true));
        ASSERT_EQ(value.size(), 7u);
    });
}

TEST(FormatGenerators, FromRegexFullmatch) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(from_regex("[a-z]+", true));
        ASSERT_FALSE(value.empty());
    });
}

// =============================================================================
// Primitive generators
// =============================================================================

TEST(PrimitiveGenerators, Nulls) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(nulls());
        (void)value; // std::monostate - just verify it doesn't throw
    });
}

// =============================================================================
// String generators with character filtering
// =============================================================================

TEST(StringGenerators, TextWithCodec) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(text({.codec = "ascii"}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithCodepointRange) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value =
            tc.draw(text({.min_codepoint = 0x41, .max_codepoint = 0x5A}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithCategories) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value =
            tc.draw(text({.categories = std::vector<std::string>{"Lu"}}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithExcludeCategories) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(
            text({.exclude_categories = std::vector<std::string>{"Cc"}}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithIncludeCharacters) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(text({.categories = std::vector<std::string>{"Lu"},
                                   .include_characters = "abc"}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithExcludeCharacters) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(text({.exclude_characters = "xyz"}));
        (void)value;
    });
}

TEST(StringGenerators, TextWithAlphabet) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(text({.alphabet = "ACGT"}));
        for (char c : value) {
            ASSERT_TRUE(c == 'A' || c == 'C' || c == 'G' || c == 'T');
        }
    });
}

TEST(StringGenerators, Characters) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value =
            tc.draw(characters({.min_codepoint = 0x41, .max_codepoint = 0x5A}));
        ASSERT_EQ(value.size(), 1u);
    });
}

// =============================================================================
// Binary generator
// =============================================================================

TEST(BinaryGenerator, BasicBinary) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(binary());
        (void)value;
    });
}

TEST(BinaryGenerator, BoundedBinary) {
    hegel::hegel([](hegel::TestCase& tc) {
        auto value = tc.draw(binary({.min_size = 4, .max_size = 16}));
        ASSERT_GE(value.size(), 4u);
        ASSERT_LE(value.size(), 16u);
    });
}

// =============================================================================
// Validation error paths not covered in test_hegel.cpp
// =============================================================================

TEST(ValidationErrors, TextAlphabetWithCharParams) {
    ASSERT_THROW(text({.min_codepoint = 0x41, .alphabet = "abc"}),
                 std::invalid_argument);
}

// =============================================================================
// note() on final replay
// =============================================================================

TEST(NoteOutput, NoteOnFailure) {
    // note() only prints during is_last_run; this just verifies it
    // doesn't throw during normal runs.
    hegel::hegel([](hegel::TestCase& tc) {
        auto x = tc.draw(integers<int>());
        tc.note("value is " + std::to_string(x));
    });
}
