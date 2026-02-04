#include <hegel/hegel.h>
#include <iostream>

#include "run_length.h"

using namespace hegel::strategies;

int main() {
    // Generate random byte sequences
    auto data = vectors(integers<uint8_t>({.min_value = 0, .max_value = 255}),
                        {.min_size = 0, .max_size = 100})
                    .generate();

    // Print the data
    std::cerr << "data size: " << data.size() << std::endl;
    {
        std::cerr << "data: [";
        for (size_t i = 0; i < data.size(); ++i) {
            if (i > 0)
                std::cerr << ", ";
            std::cerr << (int)data[i];
        }
        std::cerr << "]" << std::endl;
    }

    // Encode the data
    auto encoded = RunLengthEncoder::encode(data);

    // Decode it back
    auto decoded = RunLengthEncoder::decode(encoded);

    // Property: decode(encode(x)) == x
    if (decoded != data) {
        std::cerr << "Round-trip failed!" << std::endl;
        std::cerr << "Original size: " << data.size() << std::endl;
        std::cerr << "Decoded size: " << decoded.size() << std::endl;

        // Show first difference
        size_t min_size = std::min(data.size(), decoded.size());
        for (size_t i = 0; i < min_size; ++i) {
            if (data[i] != decoded[i]) {
                std::cerr << "First difference at index " << i << ": expected "
                          << (int)data[i] << " got " << (int)decoded[i]
                          << std::endl;
                break;
            }
        }
        if (data.size() != decoded.size()) {
            std::cerr << "Size mismatch: original=" << data.size()
                      << " decoded=" << decoded.size() << std::endl;
        }
        return 1;
    }

    // Also test the "optimized" decoder
    auto decoded_opt = RunLengthEncoder::decode_optimized(encoded);

    if (decoded_opt != data) {
        std::cerr << "Optimized round-trip failed!" << std::endl;
        std::cerr << "Original size: " << data.size() << std::endl;
        std::cerr << "Decoded_opt size: " << decoded_opt.size() << std::endl;
        return 1;
    }

    return 0;
}
