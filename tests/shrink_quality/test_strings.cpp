#include <gtest/gtest.h>

#include "common/utils.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using hegel::tests::common::minimal;
namespace gs = hegel::generators;

namespace {

    /// Count UTF-8 codepoints in `s`. Byte-wise: count bytes that are not
    /// UTF-8 continuation bytes (those match 10xxxxxx).
    size_t codepoint_count(const std::string& s) {
        size_t n = 0;
        for (char c : s) {
            if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
                ++n;
            }
        }
        return n;
    }

} // namespace

TEST(ShrinkStrings, MinimizeStringToEmpty) {
    std::string v = minimal<std::string>(
        gs::text(), [](const std::string&) { return true; });
    EXPECT_EQ(v, "");
}

TEST(ShrinkStrings, MinimizeLongerString) {
    std::string v = minimal<std::string>(gs::text(), [](const std::string& x) {
        return codepoint_count(x) >= 10;
    });
    EXPECT_EQ(v, std::string(10, '0'));
}

TEST(ShrinkStrings, MinimizeLongerListOfStrings) {
    auto v = minimal<std::vector<std::string>>(
        gs::vectors(gs::text()),
        [](const std::vector<std::string>& x) { return x.size() >= 10; });
    EXPECT_EQ(v, std::vector<std::string>(10, ""));
}
