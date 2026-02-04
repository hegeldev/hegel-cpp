#include <hegel/hegel.h>
#include <iostream>
#include <stdexcept>

using namespace hegel::st;

// Canary string that CI greps for to verify we built the right thing
void test_nix_integration_canary() {
    std::cout << "test_nix_integration_canary" << std::endl;
}

void test_addition_is_commutative() {
    hegel::hegel([] {
        auto x = integers<int>().generate();
        auto y = integers<int>().generate();
        hegel::note("x = " + std::to_string(x) + ", y = " + std::to_string(y));

        if (x + y != y + x) {
            throw std::runtime_error("addition is not commutative!");
        }
    });
    std::cout << "test_addition_is_commutative passed" << std::endl;
}

void test_zero_is_identity() {
    hegel::hegel([] {
        auto x = integers<int>().generate();
        hegel::note("x = " + std::to_string(x));

        if (x + 0 != x) {
            throw std::runtime_error("zero is not additive identity!");
        }
    });
    std::cout << "test_zero_is_identity passed" << std::endl;
}

int main() {
    test_nix_integration_canary();
    test_addition_is_commutative();
    test_zero_is_identity();
    std::cout << "All nix integration tests passed!" << std::endl;
    return 0;
}
