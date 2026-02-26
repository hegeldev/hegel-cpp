#pragma once

/**
 * @file strings.h
 * @brief String generator functions: text, binary, from_regex
 */

#include <cstdint>
#include <string>

#include "hegel/core.h"

namespace hegel::generators {

    // =============================================================================
    // Parameter structs
    // =============================================================================

    /**
     * @brief Parameters for text() strategy.
     */
    struct TextParams {
        size_t min_size = 0; ///< Minimum string length
        std::optional<size_t>
            max_size; ///< Maximum string length. Default: no limit
    };

    /**
     * @brief Parameters for binary() strategy.
     */
    struct BinaryParams {
        size_t min_size = 0; ///< Minimum size in bytes
        std::optional<size_t>
            max_size; ///< Maximum size in bytes. Default: no limit
    };

    // =============================================================================
    // Strategy declarations
    // =============================================================================

    /// @name String Strategies
    /// @{

    /**
     * @brief Generate random text strings.
     * @param params Length constraints
     * @return Generator producing random strings
     */
    Generator<std::string> text(TextParams params = {});

    /**
     * @brief Generate random binary data (byte sequences).
     * @param params Size constraints
     * @return Generator producing byte vectors
     */
    Generator<std::vector<uint8_t>> binary(BinaryParams params = {});

    /**
     * @brief Generate strings matching a regular expression.
     * @param pattern Regex pattern to match
     * @param fullmatch If true, the entire string must match the pattern; if
     * false (default), the string just needs to contain a match
     * @return Generator producing matching strings
     */
    Generator<std::string> from_regex(const std::string& pattern,
                                      bool fullmatch = false);

    /// @}

} // namespace hegel::generators
