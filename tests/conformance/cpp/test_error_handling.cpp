#include <cstdlib>
#include <iostream>

#include "hegel/hegel.h"

int main(int, char*[]) {
    try {
        hegel::hegel(
            []() {
                try {
                    auto value =
                        hegel::draw(hegel::generators::integers<int>());
                    (void)value;
                } catch (...) {
                    // The protocol test mode may cause unexpected responses.
                    // Re-throw as runtime_error so hegel can handle it.
                    throw std::runtime_error("draw failed under test mode");
                }
            },
            {.test_cases = 10});
    } catch (const std::exception& e) {
        // Error handling conformance tests inject protocol errors.
        // The library must handle them gracefully (exit 0), not crash.
        std::cerr << "Handled error: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "Handled unknown error\n";
    }

    return 0;
}
