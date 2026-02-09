#include "hegel/generators.h"
#include <base64.h>
#include <hegel/cbor.h>
#include <hegel/strategies.h>

// =============================================================================
// Strategy implementations
// =============================================================================

namespace hegel::strategies {

// Use namespace alias to avoid conflict with text() strategy function
namespace cv = hegel::cbor;
using cv::array;
using cv::boolean;
using cv::integer;
using cv::map;
using cv::map_insert;
using cv::Value;

Generator<std::monostate> nulls() {
    Value schema = map({{"type", cv::text("null")}});
    return from_function<std::monostate>([]() { return std::monostate{}; },
                                         std::move(schema));
}

Generator<bool> booleans() {
    Value schema = map({{"type", cv::text("boolean")}});
    return from_schema<bool>(std::move(schema));
}

Generator<std::string> text(TextParams params) {
    Value schema = map(
        {{"type", cv::text("string")}, {"min_size", integer(params.min_size)}});

    if (params.max_size)
        map_insert(schema, "max_size", integer(*params.max_size));

    return from_schema<std::string>(std::move(schema));
}

Generator<std::vector<uint8_t>> binary(BinaryParams params) {
    Value schema = map(
        {{"type", cv::text("binary")}, {"min_size", integer(params.min_size)}});

    if (params.max_size)
        map_insert(schema, "max_size", integer(*params.max_size));

    return from_function<std::vector<uint8_t>>(
        [schema]() -> std::vector<uint8_t> {
            std::string b64 = from_schema<std::string>(schema).generate();
            return ::hegel::impl::base64_decode(b64);
        },
        schema);
}

Generator<std::string> from_regex(const std::string& pattern, bool fullmatch) {
    Value schema = map({{"type", cv::text("regex")},
                        {"pattern", cv::text(pattern)},
                        {"fullmatch", boolean(fullmatch)}});
    return from_schema<std::string>(std::move(schema));
}

Generator<std::string> emails() {
    return from_schema<std::string>(map({{"type", cv::text("email")}}));
}

Generator<std::string> domains(DomainsParams params) {
    Value schema = map({{"type", cv::text("domain")},
                        {"max_length", integer(params.max_length)}});
    return from_schema<std::string>(std::move(schema));
}

Generator<std::string> urls() {
    return from_schema<std::string>(map({{"type", cv::text("url")}}));
}

Generator<std::string> ip_addresses(IpAddressesParams params) {
    if (params.v == 4) {
        return from_schema<std::string>(map({{"type", cv::text("ipv4")}}));
    } else if (params.v == 6) {
        return from_schema<std::string>(map({{"type", cv::text("ipv6")}}));
    } else {
        return from_schema<std::string>(
            map({{"one_of", array({map({{"type", cv::text("ipv4")}}),
                                   map({{"type", cv::text("ipv6")}})})}}));
    }
}

Generator<std::string> dates() {
    return from_schema<std::string>(map({{"type", cv::text("date")}}));
}

Generator<std::string> times() {
    return from_schema<std::string>(map({{"type", cv::text("time")}}));
}

Generator<std::string> datetimes() {
    return from_schema<std::string>(map({{"type", cv::text("datetime")}}));
}

} // namespace hegel::strategies
