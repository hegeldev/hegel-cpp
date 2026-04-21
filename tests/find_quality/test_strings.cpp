#include <gtest/gtest.h>

#include "common/utils.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

using hegel::tests::common::find_any;
namespace gs = hegel::generators;

namespace {

    /// Trim ASCII whitespace from both ends
    bool is_ws(unsigned char c) { return std::isspace(c) != 0; }

    std::string trim(const std::string& s) {
        size_t begin = 0;
        while (begin < s.size() && is_ws(static_cast<unsigned char>(s[begin])))
            ++begin;
        size_t end = s.size();
        while (end > begin && is_ws(static_cast<unsigned char>(s[end - 1])))
            --end;
        return s.substr(begin, end - begin);
    }

    bool is_ascii(const std::string& s) {
        for (char c : s) {
            if (static_cast<unsigned char>(c) > 127)
                return false;
        }
        return true;
    }

    size_t codepoint_count(const std::string& s) {
        size_t n = 0;
        for (char c : s) {
            if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
                ++n;
        }
        return n;
    }

    bool has_non_ascii(const std::string& s) {
        for (char c : s) {
            if (static_cast<unsigned char>(c) > 127)
                return true;
        }
        return false;
    }

} // namespace

TEST(FindStrings, CanProduceUnstrippedStrings) {
    find_any<std::string>(gs::text(),
                          [](const std::string& x) { return x != trim(x); });
}

TEST(FindStrings, CanProduceStrippedStrings) {
    find_any<std::string>(gs::text(),
                          [](const std::string& x) { return x == trim(x); });
}

TEST(FindStrings, CanProduceMultiLineStrings) {
    find_any<std::string>(gs::text(), [](const std::string& x) {
        return x.find('\n') != std::string::npos;
    });
}

TEST(FindStrings, CanProduceAsciiStrings) {
    find_any<std::string>(gs::text(), is_ascii);
}

TEST(FindStrings, CanProduceLongStringsWithNoAscii) {
    find_any<std::string>(gs::text({.min_size = 5}), [](const std::string& x) {
        // All *bytes* are non-ASCII (matches spirit of Rust test closely
        // enough — we're finding strings that consist entirely of multibyte
        // UTF-8 codepoints).
        if (x.empty())
            return false;
        for (char c : x) {
            if (static_cast<unsigned char>(c) <= 127)
                return false;
        }
        return true;
    });
}

TEST(FindStrings, CanProduceShortStringsWithSomeNonAscii) {
    find_any<std::string>(gs::text(), [](const std::string& x) {
        return codepoint_count(x) <= 3 && has_non_ascii(x);
    });
}

TEST(FindStrings, CanProduceLargeBinaryStrings) {
    find_any<std::vector<uint8_t>>(
        gs::binary(),
        [](const std::vector<uint8_t>& x) { return x.size() > 10; });
}

TEST(FindStrings, LongDuplicatesStrings) {
    auto pair_gen = gs::tuples(gs::text(), gs::text());
    find_any<std::tuple<std::string, std::string>>(
        pair_gen, [](const std::tuple<std::string, std::string>& t) {
            const auto& a = std::get<0>(t);
            const auto& b = std::get<1>(t);
            return codepoint_count(a) >= 5 && a == b;
        });
}
