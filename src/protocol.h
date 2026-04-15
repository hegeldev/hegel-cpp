#pragma once

#include <cstdint>
#include <hegel/options.h>
#include <nlohmann/json.hpp>
#include <vector>

// =============================================================================
// Binary Packet Protocol
// =============================================================================
namespace hegel::impl::protocol {

    inline constexpr uint32_t MAGIC = 0x4845474C; // "HEGL"
    inline constexpr uint32_t REPLY_BIT = 1U << 31;
    inline constexpr uint32_t HEADER_SIZE = 20;
    inline constexpr uint8_t TERMINATOR = 0x0A;
    inline constexpr uint8_t CLOSE_PAYLOAD = 0xFE;
    inline constexpr uint32_t CLOSE_MESSAGE_ID = (1U << 31) - 1;

    // =============================================================================
    // CRC32 (table-driven, ISO 3309)
    // =============================================================================
    uint32_t crc32(const uint8_t* data, size_t len);

    // =============================================================================
    // Packet
    // =============================================================================
    struct Packet {
        uint32_t stream;
        uint32_t message_id;
        bool is_reply;
        std::vector<uint8_t> payload;
    };

    void write_packet(int fd, uint32_t stream, uint32_t message_id,
                      bool is_reply, const std::vector<uint8_t>& payload);

    Packet read_packet(int fd);

    // =============================================================================
    // CBOR Encode / Decode
    // =============================================================================

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
        if (v.is_object()) {
            for (auto& [key, val] : v.items())
                convert_tagged_strings(val);
        }
    }

    inline std::vector<uint8_t> cbor_encode(const nlohmann::json& v) {
        return nlohmann::json::to_cbor(v);
    }

    inline nlohmann::json cbor_decode(const std::vector<uint8_t>& bytes) {
        auto result = nlohmann::json::from_cbor(
            bytes, true, true, nlohmann::json::cbor_tag_handler_t::store);
        convert_tagged_strings(result);
        return result;
    } // GCOVR_EXCL_LINE

    inline nlohmann::json cbor_decode(const uint8_t* data, size_t len) {
        auto result = nlohmann::json::from_cbor(
            data, data + len, true, true,
            nlohmann::json::cbor_tag_handler_t::store);
        convert_tagged_strings(result);
        return result;
    } // GCOVR_EXCL_LINE

    // =============================================================================
    // Protocol Debug
    // =============================================================================

    void set_protocol_debug(bool enabled);
    bool protocol_debug_enabled();

    /// Initialize protocol debug flag from verbosity + env var
    void init_protocol_debug(options::Verbosity verbosity);

} // namespace hegel::impl::protocol
