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
            max_size; ///< Maximum string length. Default: max(min_size, 100)

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
            max_size; ///< Maximum size in bytes. Default: max(min_size, 100)
    };

    /// @name Strings
    /// @{

    /**
     * @brief Generate random text strings.
     * @param params Length and character filtering constraints
     * @return Generator producing random strings
     */
    Generator<std::string> text(const TextParams& params = {});

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
     *
     * @code{.cpp}
     * // Default: the entire generated string matches the pattern,
     * // as if anchored with ^...$.
     * auto strict = from_regex("[A-Z]{2}-[0-9]{4}");
     * // e.g. "QX-8271"
     *
     * // fullmatch = false: generated string only needs to *contain* a match,
     * // so arbitrary prefix/suffix characters may surround it.
     * auto loose = from_regex("[A-Z]{2}-[0-9]{4}", false);
     * // e.g. "xx QX-8271 yy"
     * @endcode
     *
     * @param pattern Regex pattern
     * @param fullmatch If `true` (default), the entire generated string must
     *   match the pattern (equivalent to anchoring with `^` and `$`). If
     *   `false`, the generated string need only contain a substring that
     *   matches; arbitrary characters may appear before or after the match.
     * @return Generator producing strings that satisfy @p pattern under the
     *   selected match mode.
     */
    Generator<std::string> from_regex(const std::string& pattern,
                                      bool fullmatch = true);

    /// @}

} // namespace hegel::generators
