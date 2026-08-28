#include <hegel/hegel.h>
#include <iostream>
#include <stdexcept>

namespace gs = hegel::generators;

int main() {
    hegel::test([](hegel::TestCase& tc) {
        auto x = tc.draw(gs::integers<int>());
        auto y = tc.draw(gs::integers<int>());
        tc.note("x = " + std::to_string(x) + ", y = " + std::to_string(y));

        if (x + y != y + x) {
            throw std::runtime_error("addition is not commutative!");
        }
    });
    std::cout << "consumer: all tests passed" << std::endl;
    return 0;
}
