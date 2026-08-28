// String and format generators: text, characters, from_regex, emails, urls,
// domains, dates/times/datetimes, ip_addresses, uuids, binary — drawing and
// parameter validation.

#include <gtest/gtest.h>

#include <regex>
#include <stdexcept>
#include <string>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

// Draw every format/string generator end-to-end. The draw is the assertion: if
// libhegel rejects a generator's parameters it returns an error and the
// construction or draw throws, failing the property (and the test). The
// *distribution* of values is libhegel's concern; here we only confirm each
// parameterization is accepted.
TEST(Formats, DrawAll) {
    hegel::test(
        [](hegel::TestCase& tc) {
            // Standard format generators.
            (void)tc.draw(gs::emails());
            (void)tc.draw(gs::urls());
            (void)tc.draw(gs::domains());
            (void)tc.draw(gs::dates());
            (void)tc.draw(gs::times());
            (void)tc.draw(gs::datetimes());
            (void)tc.draw(gs::from_regex("[a-z]{1,5}"));
            (void)tc.draw(gs::from_regex("[A-Z]{2}", false));
            // ip_addresses: v4, v6, and the default (either) factory branches.
            (void)tc.draw(gs::ip_addresses({.v = 4}));
            (void)tc.draw(gs::ip_addresses({.v = 6}));
            (void)tc.draw(gs::ip_addresses());
            // uuids: any version, plus each explicit RFC 4122 version.
            (void)tc.draw(gs::uuids());
            (void)tc.draw(gs::uuids({.version = 4}));

            // characters() with each filtering field. Values are ones the
            // engine accepts; categories and exclude_categories are mutually
            // exclusive, so they go in separate draws.
            (void)tc.draw(gs::characters({.codec = "ascii",
                                          .min_codepoint = 97,
                                          .max_codepoint = 122,
                                          .exclude_characters = "aeiou"}));
            (void)tc.draw(gs::characters({.categories = {{"Nd"}}}));
            (void)tc.draw(gs::characters({.exclude_categories = {{"Cc"}}}));
            (void)tc.draw(gs::characters({.include_characters = "xyz"}));

            // text() routes the same fields through apply_char_fields, plus its
            // own size bounds and the dedicated alphabet branch.
            (void)tc.draw(
                gs::text({.min_size = 1, .max_size = 5, .codec = "ascii"}));
            (void)tc.draw(gs::text({.max_size = 5, .alphabet = "abc"}));

            // binary() with a max_size bound.
            (void)tc.draw(gs::binary({.max_size = 10}));
        },
        hegel::Settings{.test_cases = 50,
                        .database = hegel::Database::disabled()});
}

// alphabet cannot be combined with individual character-filtering options.
TEST(Formats, AlphabetWithCharFilteringThrows) {
    EXPECT_THROW(gs::text({.codec = "ascii", .alphabet = "abc"}),
                 std::invalid_argument);
}

// The engine validates string-generator parameters at construction; its
// rejections surface as std::invalid_argument.
TEST(Formats, InvalidRegexThrows) {
    EXPECT_THROW(gs::from_regex("("), std::invalid_argument);
}

TEST(Formats, UnknownCodecThrows) {
    EXPECT_THROW(gs::text({.codec = "no-such-codec"}), std::invalid_argument);
}

// A word-boundary anchor in a non-final position constrains the surrounding
// characters, so every generated string must contain a standalone "foo". Each
// draw is searched with the same pattern it came from, using std::regex as the
// oracle.
TEST(Formats, RegexNonFinalAnchorHonored) {
    const std::regex oracle(R"(\bfoo\b)");
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string s = tc.draw(gs::from_regex(R"(\bfoo\b)", false));
            EXPECT_TRUE(std::regex_search(s, oracle))
                << "generated string has no standalone \"foo\": " << s;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// By default the entire generated string must match the pattern, as if
// anchored with ^...$ — no surrounding characters are permitted.
TEST(Formats, RegexDefaultsToFullmatch) {
    const std::regex oracle("[A-Z]{2}-[0-9]{4}");
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string s = tc.draw(gs::from_regex("[A-Z]{2}-[0-9]{4}"));
            EXPECT_TRUE(std::regex_match(s, oracle))
                << "generated string is not a full match: " << s;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// fullmatch = false restores contains-a-match behavior: every generated
// string contains a match, but may carry arbitrary prefix/suffix characters.
// Padding around the match must actually occur across the run — otherwise this
// would pass even if the flag were ignored, since a full match also satisfies
// regex_search.
TEST(Formats, RegexFullmatchFalseAllowsContains) {
    const std::regex oracle("[A-Z]{2}-[0-9]{4}");
    bool saw_padding = false;
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string s = tc.draw(gs::from_regex("[A-Z]{2}-[0-9]{4}", false));
            EXPECT_TRUE(std::regex_search(s, oracle))
                << "generated string contains no match: " << s;
            saw_padding |= !std::regex_match(s, oracle);
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
    EXPECT_TRUE(saw_padding);
}

// uuids() produces the canonical hyphenated 8-4-4-4-12 lowercase-hex form.
TEST(Formats, UuidCanonicalForm) {
    const std::regex oracle(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string u = tc.draw(gs::uuids());
            EXPECT_TRUE(std::regex_match(u, oracle))
                << "not a canonical UUID: " << u;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// A requested version is forced into the RFC 4122 version nibble (the first
// hex digit of the third group).
TEST(Formats, UuidVersionForced) {
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string u = tc.draw(gs::uuids({.version = 4}));
            EXPECT_EQ(u[14], '4') << "version nibble not forced: " << u;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// A version outside the RFC 4122 range (1-5) is rejected at construction.
TEST(Formats, UuidInvalidVersionThrows) {
    EXPECT_THROW(gs::uuids({.version = 0}), std::invalid_argument);
    EXPECT_THROW(gs::uuids({.version = 6}), std::invalid_argument);
}

TEST(Validation, TextMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::text({.min_size = 10, .max_size = 5}),
                 std::invalid_argument);
}

TEST(Validation, BinaryMaxSizeLessThanMinSize) {
    EXPECT_THROW(gs::binary({.min_size = 10, .max_size = 5}),
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
