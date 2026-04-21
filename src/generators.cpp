#include <hegel/core.h>
#include <hegel/generators/combinators.h>
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
#include <vector>

using hegel::internal::json::ImplUtil;

namespace hegel::generators {

    namespace {

        /// Returns true if any individual character filtering param is set.
        bool has_char_params(const TextParams& params) {
            return params.codec || params.min_codepoint ||
                   params.max_codepoint || params.categories ||
                   params.exclude_categories || params.include_characters ||
                   params.exclude_characters;
        }

        /// Apply character filtering fields to a nlohmann::json schema.
        /// Used by both text() and characters().
        void apply_char_fields(
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

        class BooleansGenerator : public IGenerator<bool> {
          public:
            std::optional<BasicGenerator<bool>> as_basic() const override {
                return BasicGenerator<bool>{{{"type", "boolean"}},
                                            &default_parse_raw<bool>};
            }
        };

        class TextGenerator : public IGenerator<std::string> {
          public:
            explicit TextGenerator(TextParams params)
                : params_(std::move(params)) {
                if (params_.max_size && params_.min_size > *params_.max_size) {
                    throw std::invalid_argument(
                        "Cannot have max_size < min_size");
                }
                if (params_.alphabet && has_char_params(params_)) {
                    throw std::invalid_argument(
                        "Cannot combine alphabet with individual character "
                        "filtering options");
                }
            }

            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                nlohmann::json raw_schema = {{"type", "string"},
                                             {"min_size", params_.min_size}};
                if (params_.max_size)
                    raw_schema["max_size"] = *params_.max_size;

                if (params_.alphabet) {
                    raw_schema["categories"] = nlohmann::json::array();
                    raw_schema["include_characters"] = *params_.alphabet;
                } else {
                    apply_char_fields(
                        raw_schema, params_.codec, params_.min_codepoint,
                        params_.max_codepoint, params_.categories,
                        params_.exclude_categories, params_.include_characters,
                        params_.exclude_characters);
                }
                return BasicGenerator<std::string>{
                    ImplUtil::create(raw_schema),
                    &default_parse_raw<std::string>};
            }

          private:
            TextParams params_;
        };

        class CharactersGenerator : public IGenerator<std::string> {
          public:
            explicit CharactersGenerator(CharactersParams params)
                : params_(std::move(params)) {}

            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                nlohmann::json raw_schema = {
                    {"type", "string"}, {"min_size", 1}, {"max_size", 1}};
                apply_char_fields(
                    raw_schema, params_.codec, params_.min_codepoint,
                    params_.max_codepoint, params_.categories,
                    params_.exclude_categories, params_.include_characters,
                    params_.exclude_characters);
                return BasicGenerator<std::string>{
                    ImplUtil::create(raw_schema),
                    &default_parse_raw<std::string>};
            }

          private:
            CharactersParams params_;
        };

        class BinaryGenerator : public IGenerator<std::vector<uint8_t>> {
          public:
            explicit BinaryGenerator(BinaryParams params) : params_(params) {
                if (params_.max_size && params_.min_size > *params_.max_size) {
                    throw std::invalid_argument(
                        "Cannot have max_size < min_size");
                }
            }

            std::optional<BasicGenerator<std::vector<uint8_t>>>
            as_basic() const override {
                hegel::internal::json::json schema = {
                    {"type", "binary"}, {"min_size", params_.min_size}};
                if (params_.max_size)
                    schema["max_size"] = *params_.max_size;
                return BasicGenerator<std::vector<uint8_t>>{
                    std::move(schema),
                    [](const hegel::internal::json::json_raw_ref& raw)
                        -> std::vector<uint8_t> {
                        return ImplUtil::raw(raw).get_binary();
                    }};
            }

          private:
            BinaryParams params_;
        };

        class RegexGenerator : public IGenerator<std::string> {
          public:
            RegexGenerator(std::string pattern, bool fullmatch)
                : pattern_(std::move(pattern)), fullmatch_(fullmatch) {}

            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "regex"},
                     {"pattern", pattern_},
                     {"fullmatch", fullmatch_}},
                    &default_parse_raw<std::string>};
            }

          private:
            std::string pattern_;
            bool fullmatch_;
        };

        class EmailsGenerator : public IGenerator<std::string> {
          public:
            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "email"}}, &default_parse_raw<std::string>};
            }
        };

        class DomainsGenerator : public IGenerator<std::string> {
          public:
            explicit DomainsGenerator(DomainsParams params) : params_(params) {
                if (params_.max_length < 4 || params_.max_length > 255) {
                    throw std::invalid_argument(
                        "max_length must be between 4 and 255");
                }
            }

            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "domain"}, {"max_length", params_.max_length}},
                    &default_parse_raw<std::string>};
            }

          private:
            DomainsParams params_;
        };

        class UrlsGenerator : public IGenerator<std::string> {
          public:
            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "url"}}, &default_parse_raw<std::string>};
            }
        };

        class IpGenerator : public IGenerator<std::string> {
          public:
            explicit IpGenerator(int version) : version_(version) {}

            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", version_ == 4 ? "ipv4" : "ipv6"}},
                    &default_parse_raw<std::string>};
            }

          private:
            int version_;
        };

        class DatesGenerator : public IGenerator<std::string> {
          public:
            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "date"}}, &default_parse_raw<std::string>};
            }
        };

        class TimesGenerator : public IGenerator<std::string> {
          public:
            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "time"}}, &default_parse_raw<std::string>};
            }
        };

        class DatetimesGenerator : public IGenerator<std::string> {
          public:
            std::optional<BasicGenerator<std::string>>
            as_basic() const override {
                return BasicGenerator<std::string>{
                    {{"type", "datetime"}}, &default_parse_raw<std::string>};
            }
        };

    } // namespace

    Generator<bool> booleans() {
        return Generator<bool>(new BooleansGenerator());
    }

    Generator<std::string> text(TextParams params) {
        return Generator<std::string>(new TextGenerator(std::move(params)));
    }

    Generator<std::string> characters(const CharactersParams& params) {
        return Generator<std::string>(new CharactersGenerator(params));
    }

    Generator<std::vector<uint8_t>> binary(BinaryParams params) {
        return Generator<std::vector<uint8_t>>(new BinaryGenerator(params));
    }

    Generator<std::string> from_regex(const std::string& pattern,
                                      bool fullmatch) {
        return Generator<std::string>(new RegexGenerator(pattern, fullmatch));
    }

    Generator<std::string> emails() {
        return Generator<std::string>(new EmailsGenerator());
    }

    Generator<std::string> domains(DomainsParams params) {
        return Generator<std::string>(new DomainsGenerator(params));
    }

    Generator<std::string> urls() {
        return Generator<std::string>(new UrlsGenerator());
    }

    Generator<std::string> ip_addresses(IpAddressesParams params) {
        if (params.v && *params.v != 4 && *params.v != 6) {
            throw std::invalid_argument("ip_addresses version must be 4 or 6");
        }
        if (params.v == 4) {
            return Generator<std::string>(new IpGenerator(4));
        }
        if (params.v == 6) {
            return Generator<std::string>(new IpGenerator(6));
        }
        return one_of<std::string>(
            {Generator<std::string>(new IpGenerator(4)),
             Generator<std::string>(new IpGenerator(6))});
    }

    Generator<std::string> dates() {
        return Generator<std::string>(new DatesGenerator());
    }

    Generator<std::string> times() {
        return Generator<std::string>(new TimesGenerator());
    }

    Generator<std::string> datetimes() {
        return Generator<std::string>(new DatetimesGenerator());
    }

    HegelRandom::HegelRandom(const TestCase& tc)
        : tc_(&tc), engine_(std::nullopt) {}

    HegelRandom::HegelRandom(uint64_t seed)
        : tc_(nullptr), engine_(std::mt19937(seed)) {}

    HegelRandom::result_type HegelRandom::operator()() {
        if (engine_) {
            return (*engine_)();
        }

        hegel::internal::json::json schema = {
            {"type", "integer"},
            {"min_value", std::numeric_limits<uint32_t>::min()},
            {"max_value", std::numeric_limits<uint32_t>::max()}};

        hegel::internal::json::json response =
            internal::communicate_with_core(schema, *tc_);
        if (!response.contains("result")) {
            throw std::runtime_error("Server response missing 'result' field");
        }
        return ImplUtil::raw(response["result"]).get<uint32_t>();
    }

    Generator<HegelRandom> randoms(RandomsParams params) {
        if (params.use_true_random) {
            auto seed_gen = integers<uint64_t>();
            return compose([seed_gen](const TestCase& tc) -> HegelRandom {
                auto seed = seed_gen.do_draw(tc);
                return HegelRandom(seed);
            });
        }
        return compose(
            [](const TestCase& tc) -> HegelRandom { return HegelRandom(tc); });
    }

} // namespace hegel::generators
