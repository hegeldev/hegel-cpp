#include <gtest/gtest.h>

#include <hegel/hegel.h>
#include <type_traits>

namespace gs = hegel::generators;

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

    hegel::test([&gen](hegel::TestCase& tc) { tc.draw(gen); });
}

TEST(Compose, TrailingReturnTypeForcesGeneratorType) {
    auto gen = gs::compose([](const hegel::TestCase& tc) -> long {
        return tc.draw(gs::integers<int>());
    });
    static_assert(std::is_same_v<decltype(gen), gs::Generator<long>>);

    hegel::test([&gen](hegel::TestCase& tc) { tc.draw(gen); });
}
