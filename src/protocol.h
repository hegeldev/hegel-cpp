#pragma once

#include <cstddef>
#include <cstdint>
#include <hegel/settings.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace hegel::impl::protocol {

    inline constexpr uint64_t HEGEL_STRING_TAG = 91;

    // Convert binary values with subtype 91 (WTF-8 hegel strings) to strings.
    inline void convert_tagged_strings(nlohmann::json& v) {
        if (v.is_binary()) {
            auto& bin = v.get_binary();
            if (bin.has_subtype() && bin.subtype() == HEGEL_STRING_TAG) {
                v = std::string(bin.begin(), bin.end());
            }
            return;
        }
        if (v.is_array()) {
            for (auto& el : v)
                convert_tagged_strings(el);
            return;
        }
    }

    inline std::vector<uint8_t> cbor_encode(const nlohmann::json& v) {
        return nlohmann::json::to_cbor(v);
    }

    inline nlohmann::json cbor_decode(const uint8_t* data, size_t len) {
        auto result = nlohmann::json::from_cbor(
            data, data + len, true, true,
            nlohmann::json::cbor_tag_handler_t::store);
        convert_tagged_strings(result);
        return result;
    }

    void set_protocol_debug(bool enabled);
    bool protocol_debug_enabled();

    /// Initialize protocol debug flag from verbosity + env var
    void init_protocol_debug(Verbosity verbosity);

} // namespace hegel::impl::protocol
