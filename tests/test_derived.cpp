#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;
using hegel::draw;

// =============================================================================
// Test structs
// =============================================================================

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

struct WithOptional {
    int required;
    std::optional<std::string> maybe;
};

struct WithVector {
    std::string label;
    std::vector<int> values;
};

// =============================================================================
// Instantiation tests (no server needed)
// =============================================================================

TEST(DefaultGenerator, PrimitiveTypes) {
    EXPECT_NO_THROW(default_generator<bool>());
    EXPECT_NO_THROW(default_generator<int>());
    EXPECT_NO_THROW(default_generator<int32_t>());
    EXPECT_NO_THROW(default_generator<int64_t>());
    EXPECT_NO_THROW(default_generator<uint8_t>());
    EXPECT_NO_THROW(default_generator<size_t>());
    EXPECT_NO_THROW(default_generator<float>());
    EXPECT_NO_THROW(default_generator<double>());
    EXPECT_NO_THROW(default_generator<std::string>());
}

TEST(DefaultGenerator, ContainerTypes) {
    EXPECT_NO_THROW(default_generator<std::vector<int>>());
    EXPECT_NO_THROW(default_generator<std::set<int>>());
    EXPECT_NO_THROW((default_generator<std::map<std::string, int>>()));
    EXPECT_NO_THROW(default_generator<std::optional<int>>());
    EXPECT_NO_THROW((default_generator<std::tuple<int, std::string>>()));
    EXPECT_NO_THROW(
        (default_generator<std::variant<int, std::string, bool>>()));
}

TEST(DefaultGenerator, StructTypes) {
    EXPECT_NO_THROW(default_generator<Point>());
    EXPECT_NO_THROW(default_generator<Person>());
}

TEST(DefaultGenerator, NestedStructTypes) {
    EXPECT_NO_THROW(default_generator<Line>());
    EXPECT_NO_THROW(default_generator<WithOptional>());
    EXPECT_NO_THROW(default_generator<WithVector>());
}

TEST(DefaultGenerator, PrimitiveSchemas) {
    EXPECT_TRUE(default_generator<int>().schema().has_value());
    EXPECT_TRUE(default_generator<double>().schema().has_value());
    EXPECT_TRUE(default_generator<bool>().schema().has_value());
    EXPECT_TRUE(default_generator<std::string>().schema().has_value());
}

TEST(DefaultGenerator, StructHasNoSchema) {
    // Struct generators are function-based (no schema)
    EXPECT_FALSE(default_generator<Point>().schema().has_value());
}

TEST(DerivedGenerator, Instantiation) {
    EXPECT_NO_THROW(derive<Point>());
    EXPECT_NO_THROW(derive<Person>());
}

TEST(DerivedGenerator, WithOverrides) {
    EXPECT_NO_THROW(
        derive<Point>(field<&Point::x>(floats<double>({.min_value = 0.0}))));
    EXPECT_NO_THROW(derive<Person>(
        field<&Person::age>(integers<int>({.min_value = 0, .max_value = 120})),
        field<&Person::name>(text({.min_size = 1, .max_size = 50}))));
}

TEST(DerivedGenerator, ContainerOfStructs) {
    EXPECT_NO_THROW(vectors(default_generator<Point>()));
    EXPECT_NO_THROW(vectors(default_generator<Person>()));
}

// =============================================================================
// Property tests (require Hegel server)
// =============================================================================

TEST(DefaultGeneratorProperty, StructFieldsAreGenerated) {
    hegel::hegel([] {
        auto p = draw(default_generator<Point>());
        // Fields should be finite doubles (not NaN/Inf by default since
        // default floats generator allows them, but they should at least exist)
        (void)p.x;
        (void)p.y;
    });
}

TEST(DefaultGeneratorProperty, NestedStructWorks) {
    hegel::hegel([] {
        auto line = draw(default_generator<Line>());
        (void)line.start.x;
        (void)line.start.y;
        (void)line.end.x;
        (void)line.end.y;
    });
}

TEST(DefaultGeneratorProperty, StructWithOptional) {
    hegel::hegel([] {
        auto w = draw(default_generator<WithOptional>());
        (void)w.required;
        // maybe can be nullopt or have a value — both are valid
    });
}

TEST(DefaultGeneratorProperty, StructWithVector) {
    hegel::hegel([] {
        auto w = draw(default_generator<WithVector>());
        (void)w.label;
        (void)w.values;
    });
}

TEST(DerivedGeneratorProperty, OverriddenFieldRespectsConstraints) {
    hegel::hegel([] {
        auto gen = derive<Point>(
            field<&Point::x>(
                floats<double>({.min_value = 0.0, .max_value = 1.0})),
            field<&Point::y>(
                floats<double>({.min_value = -10.0, .max_value = 10.0})));
        auto p = draw(gen);

        ASSERT_GE(p.x, 0.0) << "x should respect min_value override";
        ASSERT_LE(p.x, 1.0) << "x should respect max_value override";
        ASSERT_GE(p.y, -10.0) << "y should respect min_value override";
        ASSERT_LE(p.y, 10.0) << "y should respect max_value override";
    });
}

TEST(DerivedGeneratorProperty, PartialOverride) {
    hegel::hegel([] {
        // Override only age, name uses default
        auto gen = derive<Person>(
            field<&Person::age>(
                integers<int>({.min_value = 0, .max_value = 120})));
        auto person = draw(gen);

        ASSERT_GE(person.age, 0) << "age should respect override min";
        ASSERT_LE(person.age, 120) << "age should respect override max";
        // name is generated by default (text()) — just check it's valid
    });
}

TEST(DerivedGeneratorProperty, VectorOfStructs) {
    hegel::hegel([] {
        auto vec = draw(
            vectors(default_generator<Point>(), {.min_size = 1, .max_size = 5}));
        ASSERT_GE(vec.size(), 1u);
        ASSERT_LE(vec.size(), 5u);
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
