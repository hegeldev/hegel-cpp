#include <hegel/core.h>
#include <hegel/generators/formats.h>
#include <hegel/generators/numeric.h>
#include <hegel/generators/primitives.h>
#include <hegel/generators/random.h>
#include <hegel/generators/strings.h>
#include <hegel/internal.h>
#include <hegel/json.h>

#include "json_impl.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using hegel::internal::json::ImplUtil;

// =============================================================================
// Primitive strategy implementations
// =============================================================================

namespace hegel::generators {

    Generator<std::monostate> nulls() {
        hegel::internal::json::json schema = {{"type", "null"}};
        return from_function<std::monostate>(
            [](TestCaseData*) { return std::monostate{}; }, std::move(schema));
    }

    Generator<bool> booleans() {
        hegel::internal::json::json schema = {{"type", "boolean"}};
        return from_schema<bool>(std::move(schema));
    }

    // =============================================================================
    // String strategy implementations
    // =============================================================================

    /// Returns true if any individual character filtering param is set.
    static bool has_char_params(const TextParams& params) {
        return params.codec || params.min_codepoint || params.max_codepoint ||
               params.categories || params.exclude_categories ||
               params.include_characters || params.exclude_characters;
    }

    /// Apply character filtering fields to a nlohmann::json schema.
    /// Used by both text() and characters().
    static void apply_char_fields(
        nlohmann::json& schema, const std::optional<std::string>& codec,
        const std::optional<uint32_t>& min_codepoint,
        const std::optional<uint32_t>& max_codepoint,
        const std::optional<std::vector<std::string>>& categories,
        const std::optional<std::vector<std::string>>& exclude_categories,
        const std::optional<std::string>& include_characters,
        const std::optional<std::string>& exclude_characters) {

        if (codec)
            schema["codec"] = *codec;
        if (min_codepoint)
            schema["min_codepoint"] = *min_codepoint;
        if (max_codepoint)
            schema["max_codepoint"] = *max_codepoint;

        if (categories)
            schema["categories"] = *categories;
        if (exclude_categories)
            schema["exclude_categories"] = *exclude_categories;

        if (include_characters)
            schema["include_characters"] = *include_characters;
        if (exclude_characters)
            schema["exclude_characters"] = *exclude_characters;
    }

    Generator<std::string> text(TextParams params) {
        if (params.max_size && params.min_size > *params.max_size) {
            throw std::invalid_argument("Cannot have max_size < min_size");
        }

        if (params.alphabet && has_char_params(params)) {
            throw std::invalid_argument(
                "Cannot combine alphabet with individual character "
                "filtering options");
        }

        nlohmann::json raw_schema = {{"type", "string"},
                                     {"min_size", params.min_size}};

        if (params.max_size)
            raw_schema["max_size"] = *params.max_size;

        if (params.alphabet) {
            // Alphabet translates to categories=[] + include_characters
            raw_schema["categories"] = nlohmann::json::array();
            raw_schema["include_characters"] = *params.alphabet;
        } else {
            apply_char_fields(raw_schema, params.codec, params.min_codepoint,
                              params.max_codepoint, params.categories,
                              params.exclude_categories,
                              params.include_characters,
                              params.exclude_characters);
        }

        return from_schema<std::string>(ImplUtil::create(raw_schema));
    }

    Generator<std::string> characters(const CharactersParams& params) {
        nlohmann::json raw_schema = {
            {"type", "string"}, {"min_size", 1}, {"max_size", 1}};

        apply_char_fields(raw_schema, params.codec, params.min_codepoint,
                          params.max_codepoint, params.categories,
                          params.exclude_categories, params.include_characters,
                          params.exclude_characters);

        return from_schema<std::string>(ImplUtil::create(raw_schema));
    }

    Generator<std::vector<uint8_t>> binary(BinaryParams params) {
        if (params.max_size && params.min_size > *params.max_size) {
            throw std::invalid_argument("Cannot have max_size < min_size");
        }

        hegel::internal::json::json schema = {{"type", "binary"},
                                              {"min_size", params.min_size}};

        if (params.max_size)
            schema["max_size"] = *params.max_size;

        return from_function<std::vector<uint8_t>>(
            [schema](TestCaseData* data) -> std::vector<uint8_t> {
                hegel::internal::json::json response =
                    internal::communicate_with_core(schema, data);
                if (!response.contains("result")) { // GCOVR_EXCL_START
                    throw std::runtime_error(
                        "Server response missing 'result' field");
                } // GCOVR_EXCL_STOP
                return ImplUtil::raw(response["result"]).get_binary();
            },
            schema);
    }

    Generator<std::string> from_regex(const std::string& pattern,
                                      bool fullmatch) {
        hegel::internal::json::json schema = {
            {"type", "regex"}, {"pattern", pattern}, {"fullmatch", fullmatch}};
        return from_schema<std::string>(std::move(schema));
    }

    // =============================================================================
    // Format string strategy implementations
    // =============================================================================

    Generator<std::string> emails() {
        return from_schema<std::string>(
            hegel::internal::json::json{{"type", "email"}});
    }

    Generator<std::string> domains(DomainsParams params) {
        if (params.max_length < 4 || params.max_length > 255) {
            throw std::invalid_argument("max_length must be between 4 and 255");
        }

        hegel::internal::json::json schema = {
            {"type", "domain"}, {"max_length", params.max_length}};
        return from_schema<std::string>(std::move(schema));
    }

    Generator<std::string> urls() {
        return from_schema<std::string>(
            hegel::internal::json::json{{"type", "url"}});
    }

    Generator<std::string> ip_addresses(IpAddressesParams params) {
        if (params.v && *params.v != 4 && *params.v != 6) {
            throw std::invalid_argument("ip_addresses version must be 4 or 6");
        }

        if (params.v == 4) {
            return from_schema<std::string>(
                hegel::internal::json::json{{"type", "ipv4"}});
        } else if (params.v == 6) {
            return from_schema<std::string>(
                hegel::internal::json::json{{"type", "ipv6"}});
        } else {
            hegel::internal::json::json one_of =
                hegel::internal::json::json::array(
                    {hegel::internal::json::json{{"type", "ipv4"}},
                     hegel::internal::json::json{{"type", "ipv6"}}});
            return from_schema<std::string>(hegel::internal::json::json{
                {"type", "one_of"}, {"generators", one_of}});
        } // GCOVR_EXCL_LINE
    }

    Generator<std::string> dates() {
        return from_schema<std::string>(
            hegel::internal::json::json{{"type", "date"}});
    }

    Generator<std::string> times() {
        return from_schema<std::string>(
            hegel::internal::json::json{{"type", "time"}});
    }

    Generator<std::string> datetimes() {
        return from_schema<std::string>(
            hegel::internal::json::json{{"type", "datetime"}});
    }

    // =============================================================================
    // Random strategy implementations
    // =============================================================================

    HegelRandom::HegelRandom(TestCaseData* data)
        : data_(data), engine_(std::nullopt) {}

    HegelRandom::HegelRandom(uint64_t seed)
        : data_(nullptr), engine_(std::mt19937(seed)) {}

    HegelRandom::result_type HegelRandom::operator()() {
        if (engine_) {
            return (*engine_)();
        }

        hegel::internal::json::json schema = {
            {"type", "integer"},
            {"min_value",
             std::numeric_limits<uint32_t>::min()}, // GCOVR_EXCL_LINE
            {"max_value", std::numeric_limits<uint32_t>::max()}};

        hegel::internal::json::json response =
            internal::communicate_with_core(schema, data_);
        if (!response.contains("result")) { // GCOVR_EXCL_START
            throw std::runtime_error("Server response missing 'result' field");
        } // GCOVR_EXCL_STOP
        return ImplUtil::raw(response["result"]).get<uint32_t>();
    }

    Generator<HegelRandom> randoms(RandomsParams params) {
        if (params.use_true_random) {
            auto seed_gen = integers<uint64_t>();
            return from_function<HegelRandom>(
                [seed_gen](TestCaseData* data) -> HegelRandom {
                    auto seed = seed_gen.do_draw(data);
                    return HegelRandom(seed);
                });
        }
        return from_function<HegelRandom>(
            [](TestCaseData* data) -> HegelRandom { // GCOVR_EXCL_LINE
                return HegelRandom(data);
            });
    }

} // namespace hegel::generators
