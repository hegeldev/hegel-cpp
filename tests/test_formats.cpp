#include <gtest/gtest.h>

#include <regex>
#include <sstream>
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

// =============================================================================
// Typed date/time generators
// =============================================================================

// dates() produces structured hegel::Date values whose fields stay within
// the documented generation range.
TEST(Datetime, DatesProduceTypedValuesInRange) {
    hegel::test(
        [](hegel::TestCase& tc) {
            hegel::Date d = tc.draw(gs::dates());
            EXPECT_GE(d.year, 1);
            EXPECT_LE(d.year, 9999);
            EXPECT_GE(d.month, 1);
            EXPECT_LE(d.month, 12);
            EXPECT_GE(d.day, 1);
            EXPECT_LE(d.day, 31);
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// times() produces structured hegel::Time values with in-range fields.
TEST(Datetime, TimesProduceTypedValuesInRange) {
    hegel::test(
        [](hegel::TestCase& tc) {
            hegel::Time t = tc.draw(gs::times());
            EXPECT_GE(t.hour, 0);
            EXPECT_LE(t.hour, 23);
            EXPECT_GE(t.minute, 0);
            EXPECT_LE(t.minute, 59);
            EXPECT_GE(t.second, 0);
            EXPECT_LE(t.second, 59);
            EXPECT_GE(t.microsecond, 0);
            EXPECT_LE(t.microsecond, 999999);
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// datetimes() produces a hegel::DateTime combining both structures.
TEST(Datetime, DatetimesProduceTypedValuesInRange) {
    hegel::test(
        [](hegel::TestCase& tc) {
            hegel::DateTime dt = tc.draw(gs::datetimes());
            EXPECT_GE(dt.date.year, 1);
            EXPECT_LE(dt.date.year, 9999);
            EXPECT_GE(dt.date.month, 1);
            EXPECT_LE(dt.date.month, 12);
            EXPECT_GE(dt.date.day, 1);
            EXPECT_LE(dt.date.day, 31);
            EXPECT_GE(dt.time.hour, 0);
            EXPECT_LE(dt.time.hour, 23);
            EXPECT_GE(dt.time.minute, 0);
            EXPECT_LE(dt.time.minute, 59);
            EXPECT_GE(dt.time.second, 0);
            EXPECT_LE(dt.time.second, 59);
            EXPECT_GE(dt.time.microsecond, 0);
            EXPECT_LE(dt.time.microsecond, 999999);
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// to_string produces the canonical ISO 8601 shape: dates are YYYY-MM-DD and
// times always carry a six-digit fractional-seconds field, including at
// microsecond == 0, so every value has exactly one serialization.
TEST(Datetime, ToStringIsCanonicalIso8601) {
    hegel::Date d{2000, 1, 2};
    EXPECT_EQ(d.to_string(), "2000-01-02");

    hegel::Time midnight{0, 0, 0, 0};
    EXPECT_EQ(midnight.to_string(), "00:00:00.000000");

    hegel::Time t{23, 59, 59, 999999};
    EXPECT_EQ(t.to_string(), "23:59:59.999999");

    hegel::DateTime dt{{1999, 12, 31}, {1, 2, 3, 42}};
    EXPECT_EQ(dt.to_string(), "1999-12-31T01:02:03.000042");
}

// operator<< prints the same canonical serialization as to_string().
TEST(Datetime, StreamInsertionMatchesToString) {
    std::ostringstream out;
    out << hegel::Date{2024, 2, 29} << " " << hegel::Time{12, 30, 0, 0} << " "
        << hegel::DateTime{{2024, 2, 29}, {12, 30, 0, 7}};
    EXPECT_EQ(out.str(),
              "2024-02-29 12:30:00.000000 2024-02-29T12:30:00.000007");
}

// Every drawn value round-trips through to_string() into the single
// canonical shape (fixed-width fields, fractional seconds always present).
TEST(Datetime, DrawnValuesSerializeToCanonicalShape) {
    const std::regex date_re(R"(^\d{4}-\d{2}-\d{2}$)");
    const std::regex time_re(R"(^\d{2}:\d{2}:\d{2}\.\d{6}$)");
    const std::regex datetime_re(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}$)");
    hegel::test(
        [&](hegel::TestCase& tc) {
            std::string d = tc.draw(gs::dates()).to_string();
            EXPECT_TRUE(std::regex_match(d, date_re)) << d;
            std::string t = tc.draw(gs::times()).to_string();
            EXPECT_TRUE(std::regex_match(t, time_re)) << t;
            std::string dt = tc.draw(gs::datetimes()).to_string();
            EXPECT_TRUE(std::regex_match(dt, datetime_re)) << dt;
        },
        hegel::Settings{.test_cases = 100,
                        .database = hegel::Database::disabled()});
}

// The comparison operators order values chronologically, field by field.
TEST(Datetime, ComparisonOperators) {
    hegel::Date d1{2000, 1, 2};
    hegel::Date d2{2000, 2, 1};
    EXPECT_TRUE(d1 == d1);
    EXPECT_TRUE(d1 != d2);
    EXPECT_TRUE(d1 < d2);
    EXPECT_TRUE(d1 <= d1);
    EXPECT_TRUE(d2 > d1);
    EXPECT_TRUE(d2 >= d2);

    hegel::Time t1{6, 0, 0, 999999};
    hegel::Time t2{6, 0, 1, 0};
    EXPECT_TRUE(t1 == t1);
    EXPECT_TRUE(t1 != t2);
    EXPECT_TRUE(t1 < t2);
    EXPECT_TRUE(t1 <= t2);
    EXPECT_TRUE(t2 > t1);
    EXPECT_TRUE(t2 >= t1);

    hegel::DateTime dt1{{2000, 1, 1}, {23, 59, 59, 999999}};
    hegel::DateTime dt2{{2000, 1, 2}, {0, 0, 0, 0}};
    EXPECT_TRUE(dt1 == dt1);
    EXPECT_TRUE(dt1 != dt2);
    EXPECT_TRUE(dt1 < dt2);
    EXPECT_TRUE(dt1 <= dt1);
    EXPECT_TRUE(dt2 > dt1);
    EXPECT_TRUE(dt2 >= dt2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
