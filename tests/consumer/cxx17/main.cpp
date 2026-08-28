// Exercises a broad set of generators — everything except default_generator,
// which needs reflection — to confirm the public headers compile and run under
// C++17 (HEGEL_REFLECTION=OFF). Instantiating tuples/collections/combinators
// matters: C++20-only constructs in those templates only surface when used.
#include <hegel/hegel.h>
#include <iostream>
#include <string>

namespace gs = hegel::generators;

int main() {
    hegel::test(
        [](hegel::TestCase& tc) {
            auto i =
                tc.draw(gs::integers<int>({.min_value = 0, .max_value = 9}));
            auto f = tc.draw(gs::floats<double>());
            auto b = tc.draw(gs::booleans());
            auto s = tc.draw(gs::text());
            auto v = tc.draw(gs::vectors(gs::integers<int>()));
            auto st = tc.draw(gs::sets(gs::integers<int>()));
            auto mp = tc.draw(gs::maps(gs::integers<int>(), gs::text()));
            auto tup = tc.draw(gs::tuples(gs::integers<int>(), gs::booleans()));
            auto opt = tc.draw(gs::optional(gs::integers<int>()));
            auto oo = tc.draw(gs::one_of({gs::just(1), gs::just(2)}));
            auto sq =
                tc.draw(gs::integers<int>({.min_value = 0, .max_value = 5})
                            .map([](int x) { return x * x; }));
            auto named = tc.draw("named", gs::integers<int>());
            auto flag = tc.draw("flag", gs::booleans());
            (void)named;
            (void)flag;
            (void)f;
            (void)b;
            (void)s;
            (void)v;
            (void)st;
            (void)mp;
            (void)tup;
            (void)opt;
            (void)oo;
            (void)sq;
            tc.assume(i >= 0);
        },
        hegel::Settings{.test_cases = 50,
                        .database = hegel::Database::disabled()});
    std::cout << "consumer: all tests passed" << std::endl;
    return 0;
}
