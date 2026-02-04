#include <functional>
#include <hegel/core.h>
#include <hegel/detail.h>
#include <hegel/detail/base64.h>
#include <hegel/embedded.h>
#include <hegel/generators.h>
#include <hegel/grouping.h>
#include <hegel/strategies.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

// POSIX headers
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// =============================================================================
// Strategy implementations
// =============================================================================

namespace hegel::strategies {
    Generator<std::monostate> nulls() {
        nlohmann::json schema = {{"type", "null"}};
        return Generator<std::monostate>([]() { return std::monostate{}; },
                                        schema.dump());
    }

    Generator<bool> booleans() {
        nlohmann::json schema = {{"type", "boolean"}};
        return from_schema<bool>(schema.dump());
    }

    Generator<std::string> text(TextParams params) {
        nlohmann::json schema = {{"type", "string"}, {"min_size", params.min_size}};

        if (params.max_size)
            schema["max_size"] = *params.max_size;

        return from_schema<std::string>(schema.dump());
    }

    Generator<std::vector<uint8_t>> binary(BinaryParams params) {
        nlohmann::json schema = {{"type", "binary"}, {"min_size", params.min_size}};

        if (params.max_size)
            schema["max_size"] = *params.max_size;

        std::string schema_str = schema.dump();
        return Generator<std::vector<uint8_t>>(
            [schema_str]() -> std::vector<uint8_t> {
                std::string b64 = from_schema<std::string>(schema_str).generate();
                return ::hegel::detail::base64_decode(b64);
            },
            schema_str);
    }

    Generator<std::string> from_regex(const std::string& pattern, bool fullmatch) {
        nlohmann::json schema = {
            {"type", "regex"}, {"pattern", pattern}, {"fullmatch", fullmatch}};
        return from_schema<std::string>(schema.dump());
    }

    Generator<std::string> emails() {
        return from_schema<std::string>(R"({"type": "email"})");
    }

    Generator<std::string> domains(DomainsParams params) {
        nlohmann::json schema = {{"type", "domain"},
                                {"max_length", params.max_length}};
        return from_schema<std::string>(schema.dump());
    }

    Generator<std::string> urls() {
        return from_schema<std::string>(R"({"type": "url"})");
    }

    Generator<std::string> ip_addresses(IpAddressesParams params) {
        if (params.v == 4) {
            return from_schema<std::string>(R"({"type": "ipv4"})");
        } else if (params.v == 6) {
            return from_schema<std::string>(R"({"type": "ipv6"})");
        } else {
            return from_schema<std::string>(
                R"({"one_of": [{"type": "ipv4"}, {"type": "ipv6"}]})");
        }
    }

    Generator<std::string> dates() {
        return from_schema<std::string>(R"({"type": "date"})");
    }

    Generator<std::string> times() {
        return from_schema<std::string>(R"({"type": "time"})");
    }

    Generator<std::string> datetimes() {
        return from_schema<std::string>(R"({"type": "datetime"})");
    }

} // namespace hegel::strategies
