#include <hegel/core.h>
#include <hegel/generators/formats.h>
#include <hegel/generators/numeric.h>
#include <hegel/generators/primitives.h>
#include <hegel/generators/random.h>
#include <hegel/generators/strings.h>
#include <hegel/internal.h>
#include <hegel/json.h>

#include "json_impl.h"

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

    Generator<std::string> text(TextParams params) {
        if (params.max_size && params.min_size > *params.max_size) {
            throw std::invalid_argument("Cannot have max_size < min_size");
        }

        hegel::internal::json::json schema = {{"type", "string"},
                                              {"min_size", params.min_size}};

        if (params.max_size)
            schema["max_size"] = *params.max_size;

        return from_schema<std::string>(std::move(schema));
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
                if (!response.contains("result")) {
                    throw std::runtime_error(
                        "Server response missing 'result' field");
                }
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
        }
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
            {"min_value", std::numeric_limits<uint32_t>::min()},
            {"max_value", std::numeric_limits<uint32_t>::max()}};

        hegel::internal::json::json response =
            internal::communicate_with_core(schema, data_);
        if (!response.contains("result")) {
            throw std::runtime_error("Server response missing 'result' field");
        }
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
            [](TestCaseData* data) -> HegelRandom {
                return HegelRandom(data);
            });
    }

} // namespace hegel::generators
