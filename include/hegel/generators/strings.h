#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hegel/core.h"

namespace hegel::generators {

    /**
     * @brief Parameters for text() generator.
     */
    struct TextParams {
        size_t min_size = 0; ///< Minimum string length
        std::optional<size_t>
            max_size; ///< Maximum string length. Default: no limit

        // Character filtering options
        std::optional<std::string> codec;      ///< Restrict to this codec
        std::optional<uint32_t> min_codepoint; ///< Minimum Unicode codepoint
        std::optional<uint32_t> max_codepoint; ///< Maximum Unicode codepoint
        std::optional<std::vector<std::string>>
            categories; ///< Whitelist of Unicode categories
        std::optional<std::vector<std::string>>
            exclude_categories; ///< Blacklist of Unicode categories
        std::optional<std::string>
            include_characters; ///< Always include these characters
        std::optional<std::string>
            exclude_characters; ///< Always exclude these characters
        std::optional<std::string>
            alphabet; ///< Fixed set of allowed characters
    };

    /**
     * @brief Parameters for characters() generator.
     *
     * Same character filtering options as TextParams except no min_size,
     * max_size, or alphabet.
     */
    struct CharactersParams {
        std::optional<std::string> codec;      ///< Restrict to this codec
        std::optional<uint32_t> min_codepoint; ///< Minimum Unicode codepoint
        std::optional<uint32_t> max_codepoint; ///< Maximum Unicode codepoint
        std::optional<std::vector<std::string>>
            categories; ///< Whitelist of Unicode categories
        std::optional<std::vector<std::string>>
            exclude_categories; ///< Blacklist of Unicode categories
        std::optional<std::string>
            include_characters; ///< Always include these characters
        std::optional<std::string>
            exclude_characters; ///< Always exclude these characters
    };

    /**
     * @brief Parameters for binary() generator.
     */
    struct BinaryParams {
        size_t min_size = 0; ///< Minimum size in bytes
        std::optional<size_t>
            max_size; ///< Maximum size in bytes. Default: no limit
    };

    /// @name Strings
    /// @{

    /**
     * @brief Generate random text strings.
     * @param params Length and character filtering constraints
     * @return Generator producing random strings
     */
    Generator<std::string> text(TextParams params = {});

    /**
     * @brief Generate single-character UTF-8 strings.
     * @param params Character filtering constraints
     * @return Generator producing single-character strings
     */
    Generator<std::string> characters(const CharactersParams& params = {});

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
