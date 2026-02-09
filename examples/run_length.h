#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// Run-length encoding with a bug when runs are very long
class RunLengthEncoder {
  public:
    using Run = std::pair<uint8_t, uint8_t>; // (value, count)

    // Encode a sequence of bytes into runs
    static std::vector<Run> encode(const std::vector<uint8_t>& data) {
        if (data.empty())
            return {};

        std::vector<Run> runs;
        uint8_t current = data[0];
        uint8_t count = 1;

        for (size_t i = 1; i < data.size(); ++i) {
            if (data[i] == current && count < 255) {
                ++count;
            } else {
                runs.push_back({current, count});
                current = data[i];
                count = 1;
            }
        }
        runs.push_back({current, count});

        return runs;
    }

    // Decode runs back to original sequence
    // BUG: Uses int for the loop counter but compares with uint8_t count
    // When count is large, the comparison can fail on some edge cases
    static std::vector<uint8_t> decode(const std::vector<Run>& runs) {
        std::vector<uint8_t> result;

        for (const auto& [value, count] : runs) {
            // BUG: Should use size_t or ensure proper comparison
            // Using signed int and comparing with unsigned can cause issues
            for (int i = 0; i < count; ++i) {
                result.push_back(value);
            }
        }

        return result;
    }

    // A buggy "optimized" decode that has an off-by-one
    static std::vector<uint8_t> decode_optimized(const std::vector<Run>& runs) {
        std::vector<uint8_t> result;

        for (const auto& [value, count] : runs) {
            // BUG: Pre-calculates size wrong when result isn't empty
            // This reserves wrong amount and can cause issues
            size_t new_size = result.size() + count;
            result.reserve(new_size);

            // BUG: Loop condition is wrong - uses <= instead of <
            // This adds one extra element per run
            for (uint8_t i = 0; i <= count; ++i) {
                if (i < count) { // Partial fix attempt, but still buggy
                    result.push_back(value);
                }
            }
        }

        return result;
    }
};
