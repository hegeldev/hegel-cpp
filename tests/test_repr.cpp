// Unit tests for the internal value printer. Each drawn value renders as one
// valid C++ expression; types without a renderable form fall back to a
// pseudo-expression.

#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <hegel/hegel.h>
#include <hegel/repr.h>

using hegel::internal::cpp_type_name;
using hegel::internal::repr;

namespace gs = hegel::generators;

namespace {
    bool contains(const std::string& haystack, const char* needle) {
        return haystack.find(needle) != std::string::npos;
    }

    // std::stod / std::stof raise out_of_range on subnormal literals;
    // strtod / strtof parse them correctly.
    double parse_double(const std::string& s) {
        return std::strtod(s.c_str(), nullptr);
    }
    float parse_float(const std::string& s) {
        return std::strtof(s.c_str(), nullptr);
    }
} // namespace

// Test types live at global scope so their rendered type names carry no
// namespace qualifier.

enum class Color : int { Red = 2, Green = 5 };

struct Streamy {
    int v;
};

std::ostream& operator<<(std::ostream& os, const Streamy& s) {
    return os << "Streamy(" << s.v << ")";
}

// Not an aggregate and no stream operator: nothing can render it.
class Opaque {
  public:
    Opaque() = default;

  private:
    int hidden_ = 0;
};

// A non-aggregate with a stream operator exercises the operator<< tier in
// every build mode.
class Wrapped {
  public:
    explicit Wrapped(int v) : v_(v) {}
    friend std::ostream& operator<<(std::ostream& os, const Wrapped& w) {
        return os << "Wrapped(" << w.v_ << ")";
    }

  private:
    int v_;
};

struct Config {
    int retries;
    std::string host;
};

// Reflectable aggregate that also has a stream operator; reflection wins.
struct Both {
    int v;
};

std::ostream& operator<<(std::ostream& os, const Both& b) {
    return os << "Both(" << b.v << ")";
}

// A struct whose field type has no rendering of its own.
struct HoldsOpaque {
    int id;
    std::vector<Opaque> parts;
};

// Aggregates that inherit. A structured binding needs every member in one
// class, so reflection cannot view either one.
struct EmptyBase {};

struct FromEmptyBase : EmptyBase {
    int d;
};

struct FieldBase {
    int b;
};

struct FromFieldBase : FieldBase {
    int d;
};

// ---------------------------------------------------------------------------
// Fundamentals
// ---------------------------------------------------------------------------

TEST(Repr, Booleans) {
    EXPECT_EQ(repr(true), "true");
    EXPECT_EQ(repr(false), "false");
}

TEST(Repr, IntegerSuffixes) {
    EXPECT_EQ(repr(10), "10");
    EXPECT_EQ(repr(-5), "-5");
    EXPECT_EQ(repr(10u), "10u");
    EXPECT_EQ(repr(10l), "10l");
    EXPECT_EQ(repr(10ul), "10ul");
    EXPECT_EQ(repr(10ll), "10ll");
    EXPECT_EQ(repr(10ull), "10ull");
}

TEST(Repr, SuffixlessIntegralsUseCasts) {
    EXPECT_EQ(repr(static_cast<short>(3)), "static_cast<short>(3)");
    EXPECT_EQ(repr(static_cast<unsigned short>(3)),
              "static_cast<unsigned short>(3)");
}

TEST(Repr, NarrowCharTypesUseCasts) {
    EXPECT_EQ(repr(static_cast<std::uint8_t>(255)),
              "static_cast<unsigned char>(255)");
    EXPECT_EQ(repr(static_cast<std::int8_t>(-1)),
              "static_cast<signed char>(-1)");
}

TEST(Repr, CharLiterals) {
    EXPECT_EQ(repr('a'), "'a'");
    EXPECT_EQ(repr('\n'), "'\\n'");
    EXPECT_EQ(repr('\t'), "'\\t'");
    EXPECT_EQ(repr('\''), "'\\''");
    EXPECT_EQ(repr('\\'), "'\\\\'");
    EXPECT_EQ(repr('\a'), "'\\007'");
    // A double quote needs no escape inside a char literal.
    EXPECT_EQ(repr('"'), "'\"'");
}

TEST(Repr, Enums) {
    std::string out = repr(Color::Red);
    EXPECT_EQ(out.rfind("static_cast<", 0), 0u) << out;
    EXPECT_TRUE(contains(out, "Color")) << out;
    EXPECT_EQ(out.substr(out.size() - 4), ">(2)") << out;
}

// ---------------------------------------------------------------------------
// Floating point
// ---------------------------------------------------------------------------

TEST(Repr, FloatingLiterals) {
    EXPECT_EQ(repr(0.5), "0.5");
    EXPECT_EQ(repr(1.5f), "1.5f");
    EXPECT_EQ(repr(-0.25f), "-0.25f");
}

TEST(Repr, IntegralValuedFloatsKeepFloatingType) {
    EXPECT_EQ(repr(2.0), "2.0");
    EXPECT_EQ(repr(2.0f), "2.0f");
    EXPECT_EQ(repr(-3.0), "-3.0");
}

TEST(Repr, LongDoubles) {
    EXPECT_EQ(repr(2.5L), "2.5L");
    EXPECT_EQ(repr(4.0L), "4.0L");
    EXPECT_EQ(cpp_type_name<long double>(), "long double");
}

TEST(Repr, SubnormalsRoundTrip) {
    double tiny = 5e-324;
    EXPECT_EQ(parse_double(repr(tiny)), tiny);
}

TEST(Repr, NonFiniteFloats) {
    EXPECT_EQ(repr(std::numeric_limits<double>::infinity()),
              "std::numeric_limits<double>::infinity()");
    EXPECT_EQ(repr(-std::numeric_limits<double>::infinity()),
              "-std::numeric_limits<double>::infinity()");
    EXPECT_EQ(repr(std::numeric_limits<double>::quiet_NaN()),
              "std::numeric_limits<double>::quiet_NaN()");
    EXPECT_EQ(repr(std::numeric_limits<float>::infinity()),
              "std::numeric_limits<float>::infinity()");
    EXPECT_EQ(repr(std::numeric_limits<float>::quiet_NaN()),
              "std::numeric_limits<float>::quiet_NaN()");
}

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

TEST(Repr, PlainString) {
    EXPECT_EQ(repr(std::string("hello")), "std::string(\"hello\")");
    EXPECT_EQ(repr(std::string()), "std::string(\"\")");
}

TEST(Repr, StringEscapes) {
    EXPECT_EQ(repr(std::string("a\"b\\c")), "std::string(\"a\\\"b\\\\c\")");
    EXPECT_EQ(repr(std::string("x\ny\tz\r")), "std::string(\"x\\ny\\tz\\r\")");
    EXPECT_EQ(repr(std::string("it's")), "std::string(\"it's\")");
}

TEST(Repr, StringNonAsciiBytesUseOctal) {
    EXPECT_EQ(repr(std::string("\xff")), "std::string(\"\\377\")");
    // Octal escapes stop at three digits, so a literal digit can follow.
    EXPECT_EQ(repr(std::string("\x01"
                               "9")),
              "std::string(\"\\0019\")");
}

TEST(Repr, StringEmbeddedNulUsesLengthCtor) {
    EXPECT_EQ(repr(std::string("a\0b", 3)), "std::string(\"a\\000b\", 3)");
}

// ---------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------

TEST(Repr, Vectors) {
    EXPECT_EQ(repr(std::vector<int>{1, 2, 3}), "std::vector<int>{1, 2, 3}");
    EXPECT_EQ(repr(std::vector<int>{}), "std::vector<int>{}");
}

TEST(Repr, NestedVectors) {
    std::vector<std::vector<int>> v{{1}, {}, {2, 3}};
    EXPECT_EQ(repr(v), "std::vector<std::vector<int>>{std::vector<int>{1}, "
                       "std::vector<int>{}, std::vector<int>{2, 3}}");
}

TEST(Repr, ByteVectorsCarryCasts) {
    std::vector<std::uint8_t> bytes{1, 255};
    EXPECT_EQ(repr(bytes),
              "std::vector<unsigned char>{static_cast<unsigned char>(1), "
              "static_cast<unsigned char>(255)}");
}

TEST(Repr, Sets) {
    EXPECT_EQ(repr(std::set<int>{2, 1}), "std::set<int>{1, 2}");
}

TEST(Repr, Maps) {
    std::map<int, std::string> m{{1, "a"}, {2, "b"}};
    EXPECT_EQ(repr(m), "std::map<int, std::string>{{1, std::string(\"a\")}, "
                       "{2, std::string(\"b\")}}");
}

TEST(Repr, Arrays) {
    std::array<int, 3> a{1, 2, 3};
    EXPECT_EQ(repr(a), "std::array<int, 3>{1, 2, 3}");
}

// ---------------------------------------------------------------------------
// Product and sum types
// ---------------------------------------------------------------------------

TEST(Repr, Pairs) {
    std::pair<int, bool> p{1, true};
    EXPECT_EQ(repr(p), "std::pair<int, bool>{1, true}");
}

TEST(Repr, Tuples) {
    std::tuple<int, bool, std::string> t{1, false, "x"};
    EXPECT_EQ(repr(t), "std::tuple<int, bool, std::string>{1, false, "
                       "std::string(\"x\")}");
}

TEST(Repr, Optionals) {
    EXPECT_EQ(repr(std::optional<int>{5}), "std::optional<int>{5}");
    EXPECT_EQ(repr(std::optional<int>{}), "std::optional<int>{}");
}

TEST(Repr, Variants) {
    std::variant<int, std::string> v = std::string("a");
    EXPECT_EQ(repr(v), "std::variant<int, std::string>{std::in_place_index<1>,"
                       " std::string(\"a\")}");
}

TEST(Repr, VariantsWithDuplicateAlternatives) {
    std::variant<int, int> v(std::in_place_index<1>, 5);
    EXPECT_EQ(repr(v), "std::variant<int, int>{std::in_place_index<1>, 5}");
}

TEST(Repr, Monostate) {
    EXPECT_EQ(repr(std::monostate{}), "std::monostate{}");
    std::variant<std::monostate, int> v;
    EXPECT_EQ(repr(v), "std::variant<std::monostate, int>"
                       "{std::in_place_index<0>, std::monostate{}}");
}

// ---------------------------------------------------------------------------
// User types: reflection, stream operator, fallback
// ---------------------------------------------------------------------------

#if HEGEL_HAS_REFLECTION
TEST(Repr, ReflectableAggregatesUseDesignatedInit) {
    Config cfg{2, "a"};
    EXPECT_EQ(repr(cfg), "Config{.retries = 2, .host = std::string(\"a\")}");
}

TEST(Repr, ReflectionBeatsStreamOperator) {
    Both b{7};
    EXPECT_EQ(repr(b), "Both{.v = 7}");
}
#endif

TEST(Repr, StreamOperatorRendersVerbatim) {
    // Not an aggregate under reflection? Streamy is an aggregate, so this
    // pins the operator<< tier only in builds without reflection.
#if !HEGEL_HAS_REFLECTION
    EXPECT_EQ(repr(Streamy{7}), "Streamy(7)");
#else
    EXPECT_EQ(repr(Streamy{7}), "Streamy{.v = 7}");
#endif
}

TEST(Repr, StreamOperatorOnlyType) {
    EXPECT_EQ(repr(Wrapped(7)), "Wrapped(7)");
}

TEST(Repr, UnprintableFallback) {
    std::string out = repr(Opaque{});
    EXPECT_EQ(out.rfind("<unprintable ", 0), 0u) << out;
    EXPECT_TRUE(contains(out, "Opaque")) << out;
    EXPECT_EQ(out.back(), '>') << out;
}

// ---------------------------------------------------------------------------
// Type spelling
// ---------------------------------------------------------------------------

TEST(Repr, TypeNames) {
    EXPECT_EQ(cpp_type_name<int>(), "int");
    EXPECT_EQ(cpp_type_name<unsigned long long>(), "unsigned long long");
    EXPECT_EQ(cpp_type_name<std::string>(), "std::string");
    EXPECT_EQ(cpp_type_name<std::vector<std::string>>(),
              "std::vector<std::string>");
    EXPECT_EQ((cpp_type_name<std::map<int, bool>>()), "std::map<int, bool>");
    EXPECT_EQ(cpp_type_name<std::optional<double>>(), "std::optional<double>");
    EXPECT_EQ((cpp_type_name<std::pair<char, float>>()),
              "std::pair<char, float>");
    EXPECT_EQ((cpp_type_name<std::tuple<int, int>>()), "std::tuple<int, int>");
    EXPECT_EQ((cpp_type_name<std::variant<int, std::monostate>>()),
              "std::variant<int, std::monostate>");
    EXPECT_EQ((cpp_type_name<std::array<int, 4>>()), "std::array<int, 4>");
}

// ---------------------------------------------------------------------------
// Round-trip properties over generated inputs
// ---------------------------------------------------------------------------

namespace {
    // Splits a rendered literal into its numeric body and trailing suffix
    // letters.
    std::pair<std::string, std::string> split_suffix(const std::string& out) {
        size_t end = out.size();
        while (end > 0 &&
               std::isalpha(static_cast<unsigned char>(out[end - 1]))) {
            --end;
        }
        return {out.substr(0, end), out.substr(end)};
    }

    // Parses the quoted part of a rendered std::string(...) expression back
    // to raw bytes, undoing the escape forms repr emits.
    std::string unescape_string_expr(const std::string& expr) {
        size_t start = expr.find('"');
        size_t end = expr.rfind('"');
        EXPECT_NE(start, std::string::npos) << expr;
        EXPECT_GT(end, start) << expr;
        std::string lit = expr.substr(start + 1, end - start - 1);
        std::string result;
        for (size_t i = 0; i < lit.size(); ++i) {
            char c = lit[i];
            if (c != '\\') {
                result += c;
                continue;
            }
            char n = lit[++i];
            switch (n) {
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case 'r':
                result += '\r';
                break;
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            default: {
                int val = 0;
                int digits = 0;
                while (i < lit.size() && digits < 3 && lit[i] >= '0' &&
                       lit[i] <= '7') {
                    val = val * 8 + (lit[i] - '0');
                    ++i;
                    ++digits;
                }
                --i;
                EXPECT_GT(digits, 0) << "bad escape in: " << expr;
                result += static_cast<char>(val);
                break;
            }
            }
        }
        return result;
    }
} // namespace

TEST(ReprProperty, Int64RoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int64_t>());
        auto [body, suffix] = split_suffix(repr(x));
        ASSERT_TRUE(suffix == "l" || suffix == "ll") << repr(x);
        ASSERT_EQ(std::stoll(body), x);
    });
}

TEST(ReprProperty, UInt64RoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<uint64_t>());
        auto [body, suffix] = split_suffix(repr(x));
        ASSERT_TRUE(suffix == "ul" || suffix == "ull") << repr(x);
        ASSERT_EQ(std::stoull(body), x);
    });
}

TEST(ReprProperty, DoubleRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::floats<double>());
        std::string out = repr(x);
        if (std::isnan(x)) {
            ASSERT_EQ(out, "std::numeric_limits<double>::quiet_NaN()");
        } else if (std::isinf(x)) {
            ASSERT_EQ(out, x > 0 ? "std::numeric_limits<double>::infinity()"
                                 : "-std::numeric_limits<double>::infinity()");
        } else {
            // The literal must parse back to the exact value and stay a
            // floating literal (never a bare integer).
            ASSERT_TRUE(out.find('.') != std::string::npos ||
                        out.find('e') != std::string::npos)
                << out;
            ASSERT_EQ(parse_double(out), x);
        }
    });
}

TEST(ReprProperty, FloatRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::floats<float>());
        std::string out = repr(x);
        if (std::isnan(x)) {
            ASSERT_EQ(out, "std::numeric_limits<float>::quiet_NaN()");
        } else if (std::isinf(x)) {
            ASSERT_EQ(out, x > 0 ? "std::numeric_limits<float>::infinity()"
                                 : "-std::numeric_limits<float>::infinity()");
        } else {
            ASSERT_EQ(out.back(), 'f') << out;
            std::string body = out.substr(0, out.size() - 1);
            ASSERT_TRUE(body.find('.') != std::string::npos ||
                        body.find('e') != std::string::npos)
                << out;
            ASSERT_EQ(parse_float(body), x);
        }
    });
}

TEST(ReprProperty, StringUnescapeRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        auto s = tc.draw(gs::text());
        std::string out = repr(s);
        ASSERT_EQ(out.rfind("std::string(", 0), 0u) << out;
        ASSERT_EQ(unescape_string_expr(out), s);
    });
}

TEST(ReprProperty, VectorRoundTrip) {
    hegel::test([](hegel::TestCase& tc) {
        auto v = tc.draw(gs::vectors(gs::integers<int32_t>()));
        std::string out = repr(v);
        ASSERT_EQ(out.rfind("std::vector<int>{", 0), 0u) << out;
        ASSERT_EQ(out.back(), '}') << out;
        std::string inner = out.substr(std::string("std::vector<int>{").size());
        inner.pop_back();
        std::vector<int32_t> parsed;
        size_t pos = 0;
        while (pos < inner.size()) {
            size_t next = inner.find(", ", pos);
            std::string tok = inner.substr(
                pos, next == std::string::npos ? next : next - pos);
            parsed.push_back(static_cast<int32_t>(std::stol(tok)));
            pos = next == std::string::npos ? inner.size() : next + 2;
        }
        ASSERT_EQ(parsed, v);
    });
}

// is_renderable_v names the same set of types repr() has a rendering for.
// The trait follows repr()'s branch conditions, so this pins the two
// together: a type repr() renders must satisfy the trait, and one it falls
// back on must not.
namespace {
    struct NoRendering {
        NoRendering() = default;

      private:
        int hidden_ = 0;
    };

    template <typename T> bool falls_back() {
        return contains(hegel::internal::repr(T{}), "<unprintable");
    }

    template <typename T> void check_agrees(const char* name) {
        EXPECT_EQ(hegel::internal::is_renderable_v<T>, !falls_back<T>())
            << "is_renderable_v disagrees with repr() for " << name;
    }
} // namespace

TEST(ReprRenderable, MatchesWhatReprProduces) {
    check_agrees<int>("int");
    check_agrees<double>("double");
    check_agrees<bool>("bool");
    check_agrees<std::string>("std::string");
    check_agrees<std::vector<int>>("std::vector<int>");
    check_agrees<std::set<int>>("std::set<int>");
    check_agrees<std::map<int, int>>("std::map<int, int>");
    check_agrees<std::optional<int>>("std::optional<int>");
    check_agrees<std::pair<int, int>>("std::pair<int, int>");
    check_agrees<std::tuple<int, int>>("std::tuple<int, int>");
    check_agrees<std::array<int, 2>>("std::array<int, 2>");
    check_agrees<NoRendering>("NoRendering");

    // The one that matters: no rendering means the trait says no.
    EXPECT_FALSE(hegel::internal::is_renderable_v<NoRendering>);
    EXPECT_TRUE(hegel::internal::is_renderable_v<int>);
}

TEST(ReprRenderable, CompoundHoldingAnUnrenderablePart) {
    EXPECT_FALSE(hegel::internal::is_renderable_v<std::vector<Opaque>>);
    EXPECT_FALSE(hegel::internal::is_renderable_v<std::set<Opaque>>);
    EXPECT_FALSE(hegel::internal::is_renderable_v<std::optional<Opaque>>);
    EXPECT_FALSE((hegel::internal::is_renderable_v<std::array<Opaque, 2>>));
    EXPECT_FALSE((hegel::internal::is_renderable_v<std::map<int, Opaque>>));
    EXPECT_FALSE((hegel::internal::is_renderable_v<std::pair<Opaque, int>>));
    EXPECT_FALSE((hegel::internal::is_renderable_v<std::tuple<int, Opaque>>));
    EXPECT_FALSE((hegel::internal::is_renderable_v<std::variant<int, Opaque>>));
    EXPECT_FALSE(hegel::internal::is_renderable_v<HoldsOpaque>);
    EXPECT_FALSE(
        hegel::internal::is_renderable_v<std::vector<std::vector<Opaque>>>);

    // A part that renders keeps the whole renderable.
    EXPECT_TRUE(hegel::internal::is_renderable_v<std::vector<int>>);
    EXPECT_TRUE(hegel::internal::is_renderable_v<std::vector<Streamy>>);
    EXPECT_TRUE(hegel::internal::is_renderable_v<std::vector<Wrapped>>);
    EXPECT_TRUE(
        (hegel::internal::is_renderable_v<std::map<std::string, Streamy>>));
}

TEST(ReprRenderable, UnrenderablePartSitsInsideTheValue) {
    std::string out = repr(std::vector<Opaque>{Opaque{}});
    EXPECT_TRUE(contains(out, "<unprintable")) << out;
    // The value itself renders; only the part inside it does not.
    EXPECT_EQ(out.rfind("std::vector<Opaque>{", 0), 0u) << out;
}

// An aggregate that inherits is one reflection cannot view. Asking the trait
// must answer false rather than fail to compile.
TEST(ReprRenderable, InheritingAggregateIsNotRenderable) {
    EXPECT_FALSE(hegel::internal::is_renderable_v<FromEmptyBase>);
    EXPECT_FALSE(hegel::internal::is_renderable_v<FromFieldBase>);
    EXPECT_TRUE(contains(repr(FromFieldBase{{1}, 2}), "<unprintable"));

#if HEGEL_HAS_REFLECTION
    // The same struct without the base class stays renderable.
    EXPECT_TRUE(hegel::internal::is_renderable_v<Config>);
#endif
}
