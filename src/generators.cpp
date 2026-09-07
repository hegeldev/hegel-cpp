#include <hegel/core.h>
#include <hegel/generators/combinators.h>
#include <hegel/generators/formats.h>
#include <hegel/generators/numeric.h>
#include <hegel/generators/primitives.h>
#include <hegel/generators/random.h>
#include <hegel/generators/strings.h>
#include <hegel/internal.h>

#include <engine.h>
#include <hegel.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace hegel::generators {

    namespace {

        // Engine default when the caller sets no max_size, mirroring the
        // native generators' sizing.
        uint64_t default_max_size(size_t min_size,
                                  const std::optional<size_t>& max_size) {
            if (max_size) {
                return *max_size;
            }
            return std::max<uint64_t>(min_size, 100);
        }

        /// Returns true if any individual character filtering param is set.
        bool has_char_params(const TextParams& params) {
            return params.codec || params.min_codepoint ||
                   params.max_codepoint || params.categories ||
                   params.exclude_categories || params.include_characters ||
                   params.exclude_characters;
        }

        // Shared owner of a validated hegel_string_generator_t. Immutable
        // and shareable across test cases and threads; released once the
        // last generator holding it is destroyed.
        class StringGeneratorHandle {
          public:
            explicit StringGeneratorHandle(hegel_string_generator_t* generator)
                : generator_(generator) {}
            ~StringGeneratorHandle() {
                impl::string_generator_free(generator_);
            }
            StringGeneratorHandle(const StringGeneratorHandle&) = delete;
            StringGeneratorHandle&
            operator=(const StringGeneratorHandle&) = delete;

            const hegel_string_generator_t* get() const { return generator_; }

          private:
            hegel_string_generator_t* generator_;
        };

        // Base for every generator that draws through a string-generator
        // handle (text, characters, regex, emails, urls, domains).
        class StringDrawGenerator : public IGenerator<std::string> {
          public:
            explicit StringDrawGenerator(hegel_string_generator_t* generator)
                : handle_(std::make_shared<StringGeneratorHandle>(generator)) {}

            std::string do_draw(const TestCase& tc) const override {
                return impl::draw_string(tc, handle_->get());
            }

          private:
            std::shared_ptr<StringGeneratorHandle> handle_;
        };

        hegel_string_generator_t* build_text_handle(const TextParams& params) {
            impl::TextGeneratorParams cparams;
            cparams.min_size = params.min_size;
            cparams.max_size =
                default_max_size(params.min_size, params.max_size);

            // alphabet is sugar for "no categories, exactly these
            // characters".
            static const std::vector<std::string> no_categories;
            if (params.alphabet) {
                cparams.categories = &no_categories;
                cparams.include_characters = &*params.alphabet;
            } else {
                if (params.codec)
                    cparams.codec = params.codec->c_str();
                if (params.min_codepoint)
                    cparams.min_codepoint = *params.min_codepoint;
                if (params.max_codepoint)
                    cparams.max_codepoint = *params.max_codepoint;
                if (params.categories)
                    cparams.categories = &*params.categories;
                if (params.exclude_categories)
                    cparams.exclude_categories = &*params.exclude_categories;
                if (params.include_characters)
                    cparams.include_characters = &*params.include_characters;
                if (params.exclude_characters)
                    cparams.exclude_characters = &*params.exclude_characters;
            }
            return impl::string_generator_text(cparams);
        }

        class TextGenerator : public StringDrawGenerator {
          public:
            explicit TextGenerator(const TextParams& params)
                : StringDrawGenerator(validate_and_build(params)) {}

          private:
            static hegel_string_generator_t*
            validate_and_build(const TextParams& params) {
                if (params.max_size && params.min_size > *params.max_size) {
                    throw std::invalid_argument(
                        "Cannot have max_size < min_size");
                }
                if (params.alphabet && has_char_params(params)) {
                    throw std::invalid_argument(
                        "Cannot combine alphabet with individual character "
                        "filtering options");
                }
                return build_text_handle(params);
            }
        };

        class CharactersGenerator : public StringDrawGenerator {
          public:
            explicit CharactersGenerator(const CharactersParams& params)
                : StringDrawGenerator(build(params)) {}

          private:
            static hegel_string_generator_t*
            build(const CharactersParams& params) {
                TextParams text_params{};
                text_params.min_size = 1;
                text_params.max_size = 1;
                text_params.codec = params.codec;
                text_params.min_codepoint = params.min_codepoint;
                text_params.max_codepoint = params.max_codepoint;
                text_params.categories = params.categories;
                text_params.exclude_categories = params.exclude_categories;
                text_params.include_characters = params.include_characters;
                text_params.exclude_characters = params.exclude_characters;
                return build_text_handle(text_params);
            }
        };

        class BinaryGenerator : public IGenerator<std::vector<uint8_t>> {
          public:
            explicit BinaryGenerator(const BinaryParams& params)
                : min_size_(params.min_size),
                  max_size_(
                      default_max_size(params.min_size, params.max_size)) {
                if (params.max_size && params.min_size > *params.max_size) {
                    throw std::invalid_argument(
                        "Cannot have max_size < min_size");
                }
            }

            std::vector<uint8_t> do_draw(const TestCase& tc) const override {
                return impl::draw_bytes(tc, min_size_, max_size_);
            }

          private:
            uint64_t min_size_;
            uint64_t max_size_;
        };

        class BooleansGenerator : public IGenerator<bool> {
          public:
            bool do_draw(const TestCase& tc) const override {
                return internal::draw_boolean(tc, 0.5);
            }
        };

        /// ISO 8601 `YYYY-MM-DD`.
        std::string format_date(const hegel_date_t& d) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u",
                          static_cast<int>(d.year), d.month, d.day);
            return buf;
        }

        /// ISO 8601 `HH:MM:SS[.fffffffff]` — the fractional part is present
        /// iff the nanosecond is nonzero.
        std::string format_time(const hegel_time_t& t) {
            char buf[24];
            if (t.nanosecond != 0) {
                std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%09u", t.hour,
                              t.minute, t.second,
                              static_cast<unsigned>(t.nanosecond));
            } else {
                std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u", t.hour,
                              t.minute, t.second);
            }
            return buf;
        }

        class DatesGenerator : public IGenerator<std::string> {
          public:
            std::string do_draw(const TestCase& tc) const override {
                return format_date(impl::draw_date(tc));
            }
        };

        class TimesGenerator : public IGenerator<std::string> {
          public:
            std::string do_draw(const TestCase& tc) const override {
                return format_time(impl::draw_time(tc));
            }
        };

        class DatetimesGenerator : public IGenerator<std::string> {
          public:
            std::string do_draw(const TestCase& tc) const override {
                hegel_datetime_t dt = impl::draw_datetime(tc);
                return format_date(dt.date) + "T" + format_time(dt.time);
            }
        };

        class IpGenerator : public IGenerator<std::string> {
          public:
            explicit IpGenerator(int version) : version_(version) {}

            std::string do_draw(const TestCase& tc) const override {
                return version_ == 4 ? impl::draw_ipv4(tc)
                                     : impl::draw_ipv6(tc);
            }

          private:
            int version_;
        };

        class UuidGenerator : public IGenerator<std::string> {
          public:
            explicit UuidGenerator(std::optional<uint8_t> version)
                : version_(version) {}

            std::string do_draw(const TestCase& tc) const override {
                return impl::draw_uuid(tc, version_);
            }

          private:
            std::optional<uint8_t> version_;
        };

    } // namespace

    Generator<bool> booleans() {
        return Generator<bool>(new BooleansGenerator());
    }

    Generator<std::string> text(const TextParams& params) {
        return Generator<std::string>(new TextGenerator(params));
    }

    Generator<std::string> characters(const CharactersParams& params) {
        return Generator<std::string>(new CharactersGenerator(params));
    }

    Generator<std::vector<uint8_t>> binary(BinaryParams params) {
        return Generator<std::vector<uint8_t>>(new BinaryGenerator(params));
    }

    Generator<std::string> from_regex(const std::string& pattern,
                                      bool fullmatch) {
        return Generator<std::string>(new StringDrawGenerator(
            impl::string_generator_regex(pattern.c_str(), fullmatch)));
    }

    Generator<std::string> emails() {
        return Generator<std::string>(
            new StringDrawGenerator(impl::string_generator_email()));
    }

    Generator<std::string> domains(DomainsParams params) {
        if (params.max_length < 4 || params.max_length > 255) {
            throw std::invalid_argument("max_length must be between 4 and 255");
        }
        return Generator<std::string>(new StringDrawGenerator(
            impl::string_generator_domain(params.max_length)));
    }

    Generator<std::string> urls() {
        return Generator<std::string>(
            new StringDrawGenerator(impl::string_generator_url()));
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

    Generator<std::string> uuids(UuidsParams params) {
        std::optional<uint8_t> version;
        if (params.version) {
            if (*params.version < 1 || *params.version > 5) {
                throw std::invalid_argument(
                    "uuids version must be between 1 and 5");
            }
            version = static_cast<uint8_t>(*params.version);
        }
        return Generator<std::string>(new UuidGenerator(version));
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
        return static_cast<result_type>(
            internal::draw_integer(*tc_, std::numeric_limits<uint32_t>::min(),
                                   std::numeric_limits<uint32_t>::max()));
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
