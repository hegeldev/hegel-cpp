#include <gtest/gtest.h>

#include <stdexcept>

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
            (void)tc.draw(gs::from_regex("[A-Z]{2}", true));
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
