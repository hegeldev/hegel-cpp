// Combinator and composition generators: compose, map / flat_map / filter,
// one_of / variant / optional, just / sampled_from, deferred, and the
// function-backed (CompositeGenerator) fallback path.

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include <hegel/hegel.h>

namespace gs = hegel::generators;

// ---------------------------------------------------------------------------
// compose
// ---------------------------------------------------------------------------

namespace {
    struct Point {
        int x;
        int y;
    };
} // namespace

TEST(Compose, DeducesTypeFromLambdaReturn) {
    auto gen = gs::compose([](const hegel::TestCase& tc) {
        int x = tc.draw(gs::integers<int>());
        int y = tc.draw(gs::integers<int>());
        return Point{x, y};
    });
    static_assert(std::is_same_v<decltype(gen), gs::Generator<Point>>);
    static_assert(std::is_same_v<decltype(gen)::value_type, Point>);

    hegel::test([&gen](hegel::TestCase& tc) { tc.draw(gen); });
}

TEST(Compose, TrailingReturnTypeForcesGeneratorType) {
    auto gen = gs::compose([](const hegel::TestCase& tc) -> long {
        return tc.draw(gs::integers<int>());
    });
    static_assert(std::is_same_v<decltype(gen), gs::Generator<long>>);

    hegel::test([&gen](hegel::TestCase& tc) { tc.draw(gen); });
}

// ---------------------------------------------------------------------------
// deferred
// ---------------------------------------------------------------------------

namespace {
    // A recursive tree: either a leaf holding an int, or a branch holding two
    // subtrees. Recursion is expressed through deferred().
    struct Tree {
        int leaf = 0;
        std::vector<Tree> children;
    };
} // namespace

// A self-referential generator built with deferred() produces values. Depth is
// bounded by the engine's draw budget, so the run completes without failing.
TEST(Deferred, RecursiveTreeGenerates) {
    auto tree = gs::deferred<Tree>();

    auto leaf = gs::integers<int>().map([](int v) { return Tree{v, {}}; });
    auto branch = gs::compose([tree](const hegel::TestCase& tc) -> Tree {
        Tree t;
        t.children.push_back(tc.draw(tree.generator()));
        t.children.push_back(tc.draw(tree.generator()));
        return t;
    });
    tree.set(gs::one_of<Tree>({leaf, branch}));

    EXPECT_NO_THROW(hegel::test(
        [&](hegel::TestCase& tc) { (void)tc.draw(tree.generator()); },
        hegel::Settings{
            .test_cases = 50,
            .derandomize = true,
            .database = hegel::Database::disabled(),
            .suppress_health_check = {hegel::HealthCheck::FilterTooMuch}}));
}

// Drawing from a handle before set() has supplied an implementation is an
// error.
TEST(Deferred, DrawBeforeSetThrows) {
    auto def = gs::deferred<int>();
    auto handle = def.generator();
    EXPECT_THROW(
        hegel::test([&](hegel::TestCase& tc) { (void)tc.draw(handle); },
                    hegel::Settings{.test_cases = 1,
                                    .derandomize = true,
                                    .database = hegel::Database::disabled()}),
        std::runtime_error);
}

// set() installs the implementation exactly once; a second call is an error.
TEST(Deferred, SetTwiceThrows) {
    auto def = gs::deferred<int>();
    def.set(gs::just(1));
    EXPECT_THROW(def.set(gs::just(2)), std::runtime_error);
}

// ---------------------------------------------------------------------------
// just / sampled_from with opaque (non-serializable) types
// ---------------------------------------------------------------------------

TEST(JustOverload, StringLiteralDefaultsToString) {
    auto g = gs::just("hello");
    static_assert(
        std::is_same_v<decltype(g), gs::Generator<std::string>>,
        "just(\"hello\") should deduce Generator<std::string> via the "
        "const char* overload");
}

TEST(JustOverload, ExplicitCharPtrKeepsCharPtr) {
    auto g = gs::just<const char*>("hello");
    static_assert(std::is_same_v<decltype(g), gs::Generator<const char*>>,
                  "just<const char*>(\"hello\") should select the template and "
                  "yield Generator<const char*>");
}

namespace {
    // A type with no public fields for reflect-cpp and no default
    // constructor - unambiguously non-serializable. just() and
    // sampled_from() never hand T to the engine (they only draw an index,
    // if anything), so this must still work.
    class OpaqueHandle {
      public:
        explicit OpaqueHandle(int id) : id_(id) {}
        int id() const { return id_; }
        bool operator==(const OpaqueHandle& other) const {
            return id_ == other.id_;
        }

      private:
        int id_;
    };
} // namespace

TEST(NonSerializable, JustWorksWithOpaqueType) {
    auto gen = gs::just(OpaqueHandle{42});
    hegel::test([&gen](hegel::TestCase& tc) {
        OpaqueHandle drawn = tc.draw(gen);
        EXPECT_EQ(drawn.id(), 42);
    });
}

TEST(NonSerializable, SampledFromWorksWithOpaqueType) {
    std::vector<OpaqueHandle> options;
    options.emplace_back(1);
    options.emplace_back(2);
    options.emplace_back(3);
    auto gen = gs::sampled_from(options);
    hegel::test([&gen](hegel::TestCase& tc) {
        OpaqueHandle drawn = tc.draw(gen);
        EXPECT_TRUE(drawn == OpaqueHandle{1} || drawn == OpaqueHandle{2} ||
                    drawn == OpaqueHandle{3});
    });
}

TEST(Validation, OneOfEmptyVector) {
    EXPECT_THROW(gs::one_of(std::vector<gs::Generator<int>>{}),
                 std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Function-backed (CompositeGenerator) fallback path
// ---------------------------------------------------------------------------

namespace {
    gs::Generator<int> always_even() {
        return gs::integers<int>({.min_value = 0, .max_value = 100})
            .filter([](const int& x) { return x % 2 == 0; });
    }

    hegel::Settings fast() {
        // The even-only filter rejects ~half its draws; across a composite of
        // several elements that adds up, so suppress the health checks.
        return hegel::Settings{.test_cases = 50,
                               .database = hegel::Database::disabled(),
                               .suppress_health_check =
                                   hegel::all_health_checks()};
    }
} // namespace

TEST(CompositeFallback, SetsWithFunctionBackedElement) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto s = tc.draw(
                gs::sets(always_even(), {.min_size = 1, .max_size = 5}));
            EXPECT_GE(s.size(), 1u);
            EXPECT_LE(s.size(), 5u);
            for (int x : s) {
                EXPECT_EQ(x % 2, 0) << "set element should be even";
            }
        },
        fast());
}

TEST(CompositeFallback, MapsWithFunctionBackedKey) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto m = tc.draw(gs::maps(always_even(), gs::integers<int>(),
                                      {.min_size = 1, .max_size = 5}));
            EXPECT_GE(m.size(), 1u);
            EXPECT_LE(m.size(), 5u);
            for (const auto& [k, v] : m) {
                EXPECT_EQ(k % 2, 0) << "map key should be even";
            }
        },
        fast());
}

TEST(CompositeFallback, TuplesWithFunctionBackedElement) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto t = tc.draw(gs::tuples(always_even(), gs::booleans()));
            EXPECT_GE(std::get<0>(t), 0);
            EXPECT_LE(std::get<0>(t), 100);
            EXPECT_EQ(std::get<0>(t) % 2, 0) << "tuple element should be even";
        },
        fast());
}

TEST(CompositeFallback, VariantWithFunctionBackedBranch) {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto v = tc.draw(gs::variant(always_even(), gs::booleans()));
            EXPECT_LT(v.index(), 2u);
            if (v.index() == 0) {
                EXPECT_EQ(std::get<0>(v) % 2, 0)
                    << "variant int should be even";
            }
        },
        fast());
}

TEST(CompositeFallback, SampledFromEmptyThrows) {
    EXPECT_THROW(gs::sampled_from(std::vector<int>{}), std::invalid_argument);
}

TEST(CompositeFallback, UniqueVectorsRequireEqualityComparable) {
    struct NoEq {
        int value;
    };
    auto elements = gs::compose(
        [](const hegel::TestCase& tc) { return NoEq{tc.draw(always_even())}; });
    EXPECT_THROW(gs::vectors(elements, {.unique = true}),
                 std::invalid_argument);
}

TEST(CompositeFallback, SampledFromStringLiterals) {
    hegel::test(
        [](hegel::TestCase& tc) {
            // The initializer_list<const char*> overload yields std::string.
            std::string s = tc.draw(gs::sampled_from({"red", "green", "blue"}));
            EXPECT_TRUE(s == "red" || s == "green" || s == "blue");
        },
        fast());
}

TEST(CompositeFallback, UnsatisfiableUniqueMapIsRejected) {
    hegel::test(
        [](hegel::TestCase& tc) {
            // Only two possible keys (0, 1) but at least five required.
            (void)tc.draw(
                gs::maps(gs::integers<int>({.min_value = 0, .max_value = 1}),
                         gs::integers<int>(), {.min_size = 5}));
        },
        hegel::Settings{.test_cases = 10,
                        .database = hegel::Database::disabled(),
                        .suppress_health_check = hegel::all_health_checks()});
}
