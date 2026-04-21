#include <gtest/gtest.h>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

struct Point {
    double x;
    double y;
};

struct Person {
    std::string name;
    int age;
};

struct Line {
    Point start;
    Point end;
};

struct AllDefaults {
    bool b;
    int i;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    size_t sz;
    float f;
    double d;
    std::string s;
    std::vector<int> vec;
    std::set<int> st;
    std::map<std::string, int> mp;
    std::optional<int> opt;
    std::tuple<int, std::string> tup;
    std::variant<int, std::string, bool> var;
};

TEST(DefaultGenerator, PrimitiveTypes) {
    EXPECT_NO_THROW(gs::default_generator<bool>());
    EXPECT_NO_THROW(gs::default_generator<int>());
    EXPECT_NO_THROW(gs::default_generator<int32_t>());
    EXPECT_NO_THROW(gs::default_generator<int64_t>());
    EXPECT_NO_THROW(gs::default_generator<uint8_t>());
    EXPECT_NO_THROW(gs::default_generator<size_t>());
    EXPECT_NO_THROW(gs::default_generator<float>());
    EXPECT_NO_THROW(gs::default_generator<double>());
    EXPECT_NO_THROW(gs::default_generator<std::string>());
}

TEST(DefaultGenerator, ContainerTypes) {
    EXPECT_NO_THROW(gs::default_generator<std::vector<int>>());
    EXPECT_NO_THROW(gs::default_generator<std::set<int>>());
    EXPECT_NO_THROW((gs::default_generator<std::map<std::string, int>>()));
    EXPECT_NO_THROW(gs::default_generator<std::optional<int>>());
    EXPECT_NO_THROW((gs::default_generator<std::tuple<int, std::string>>()));
    EXPECT_NO_THROW(
        (gs::default_generator<std::variant<int, std::string, bool>>()));
}

TEST(DefaultGenerator, StructTypes) {
    EXPECT_NO_THROW(gs::default_generator<Point>());
    EXPECT_NO_THROW(gs::default_generator<Person>());
}

TEST(DefaultGenerator, PrimitiveSchemas) {
    EXPECT_TRUE(gs::default_generator<int>().schema().has_value());
    EXPECT_TRUE(gs::default_generator<double>().schema().has_value());
    EXPECT_TRUE(gs::default_generator<bool>().schema().has_value());
    EXPECT_TRUE(gs::default_generator<std::string>().schema().has_value());
}

TEST(DefaultGenerator, StructHasNoSchema) {
    // Struct generators are function-based (no schema)
    EXPECT_FALSE(gs::default_generator<Point>().schema().has_value());
}

TEST(Map, PreservesSchemaOnBasicGenerator) {
    // map() on a schema-backed generator should keep the schema (the
    // transformation is composed into the client-side parse step).
    auto squared =
        gs::integers<int>({.min_value = 0, .max_value = 10}).map([](int x) {
            return x * x;
        });
    EXPECT_TRUE(squared.schema().has_value());
}

TEST(Map, DropsSchemaOnFunctionBacked) {
    // map() on a function-backed generator (no as_basic()) falls back to a
    // function-backed result.
    auto struct_gen = gs::default_generator<Point>();
    EXPECT_FALSE(struct_gen.schema().has_value());
    auto mapped = struct_gen.map([](const Point& p) { return p.x; });
    EXPECT_FALSE(mapped.schema().has_value());
}

TEST(DefaultGenerator, Instantiation) {
    EXPECT_NO_THROW(gs::default_generator<Point>());
    EXPECT_NO_THROW(gs::default_generator<Person>());
}

TEST(DefaultGenerator, WithOverrides) {
    EXPECT_NO_THROW(gs::default_generator<Point>().override(
        gs::field<&Point::x>(gs::floats<double>({.min_value = 0.0}))));
    EXPECT_NO_THROW(gs::default_generator<Person>().override(
        gs::field<&Person::age>(
            gs::integers<int>({.min_value = 0, .max_value = 120})),
        gs::field<&Person::name>(gs::text({.min_size = 1, .max_size = 50}))));
}

TEST(DefaultGenerator, ContainerOfStructs) {
    EXPECT_NO_THROW(gs::vectors(gs::default_generator<Point>()));
    EXPECT_NO_THROW(gs::vectors(gs::default_generator<Person>()));
}

TEST(DefaultGeneratorProperty, StructFieldsAreGenerated) {
    hegel::test([](hegel::TestCase& tc) {
        auto p = tc.draw(gs::default_generator<Point>());
        // Fields should be finite doubles (not NaN/Inf by default since
        // default floats generator allows them, but they should at least exist)
        (void)p.x;
        (void)p.y;
    });
}

TEST(DefaultGeneratorProperty, NestedStructWorks) {
    hegel::test([](hegel::TestCase& tc) {
        auto line = tc.draw(gs::default_generator<Line>());
        (void)line.start.x;
        (void)line.start.y;
        (void)line.end.x;
        (void)line.end.y;
    });
}

TEST(DefaultGeneratorProperty, OverriddenFieldRespectsConstraints) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen = gs::default_generator<Point>().override(
            gs::field<&Point::x>(
                gs::floats<double>({.min_value = 0.0, .max_value = 0.0})),
            gs::field<&Point::y>(
                gs::floats<double>({.min_value = -10.0, .max_value = 10.0})));
        auto p = tc.draw(gen);

        ASSERT_EQ(p.x, 0.0) << "x should respect value override";
        ASSERT_GE(p.y, -10.0) << "y should respect min_value override";
        ASSERT_LE(p.y, 10.0) << "y should respect max_value override";
    });
}

TEST(DefaultGeneratorProperty, PartialOverride) {
    hegel::test([](hegel::TestCase& tc) {
        // Override only age, name uses default
        auto gen =
            gs::default_generator<Person>().override(gs::field<&Person::age>(
                gs::integers<int>({.min_value = 0, .max_value = 120})));
        auto person = tc.draw(gen);

        ASSERT_GE(person.age, 0) << "age should respect override min";
        ASSERT_LE(person.age, 120) << "age should respect override max";
    });
}

TEST(DefaultGeneratorProperty, VectorOfStructs) {
    hegel::test([](hegel::TestCase& tc) {
        auto vec = tc.draw(gs::vectors(gs::default_generator<Point>(),
                                       {.min_size = 1, .max_size = 5}));
        ASSERT_GE(vec.size(), 1u);
        ASSERT_LE(vec.size(), 5u);
    });
}

// =============================================================================
// Combinator tests (map, filter, flat_map on derived generators)
// =============================================================================

TEST(DefaultGeneratorProperty, MapOnStruct) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen =
            gs::default_generator<Point>()
                .override(gs::field<&Point::x>(gs::integers<int>(
                              {.min_value = 0, .max_value = 100})),
                          gs::field<&Point::y>(gs::integers<int>(
                              {.min_value = 0, .max_value = 100})))
                .map([](Point p) { return std::sqrt(p.x * p.x + p.y * p.y); });
        auto distance = tc.draw(gen);
        ASSERT_GE(distance, 0.0) << "Distance should be non-negative";
    });
}

TEST(DefaultGeneratorProperty, FlatMapOnStruct) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen = gs::default_generator<Point>()
                       .override(gs::field<&Point::x>(gs::integers<uint16_t>(
                           {.min_value = 0, .max_value = 100})))
                       .flat_map([](Point p) {
                           int bound = static_cast<uint16_t>(std::abs(p.x));
                           return gs::integers<int>(
                               {.min_value = 0, .max_value = bound});
                       });
        auto val = tc.draw(gen);
        ASSERT_GE(val, 0);
    });
}

TEST(DefaultGeneratorProperty, OneOfWithStructs) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen = gs::one_of<Point>({
            gs::default_generator<Point>(),
            gs::default_generator<Point>().override(
                gs::field<&Point::x>(gs::just(0.0)),
                gs::field<&Point::y>(gs::just(0.0))),
        });
        auto p = tc.draw(gen);
        (void)p.x;
        (void)p.y;
    });
}

TEST(DefaultGeneratorProperty, StructWithAllDefaultGenerators) {
    hegel::test([](hegel::TestCase& tc) {
        auto a = tc.draw(gs::default_generator<AllDefaults>());
        EXPECT_TRUE(a.b == true || a.b == false);
        EXPECT_EQ(a.i, a.i);
        EXPECT_EQ(a.i32, a.i32);
        EXPECT_EQ(a.i64, a.i64);
        EXPECT_EQ(a.u8, a.u8);
        EXPECT_EQ(a.sz, a.sz);
        EXPECT_TRUE(std::isnan(a.f) || !std::isnan(a.f));
        EXPECT_TRUE(std::isnan(a.d) || !std::isnan(a.d));
        ASSERT_GE(a.s.size(), 0);
        ASSERT_GE(a.vec.size(), 0);
        ASSERT_GE(a.st.size(), 0);
        ASSERT_GE(a.mp.size(), a.mp.size());
        ASSERT_TRUE(a.opt.has_value() || !a.opt.has_value());
        int tup_first = std::get<0>(a.tup);
        EXPECT_EQ(tup_first, tup_first);
        ASSERT_GE(std::get<1>(a.tup).size(), 0);
        EXPECT_LT(a.var.index(), 3u);
    });
}

TEST(DefaultGeneratorProperty, OptionalOfStruct) {
    hegel::test([](hegel::TestCase& tc) {
        auto gen = gs::optional(gs::default_generator<Point>());
        auto maybe_point = tc.draw(gen);
        if (maybe_point.has_value()) {
            (void)maybe_point->x;
            (void)maybe_point->y;
        }
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
