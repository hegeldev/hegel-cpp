#pragma once

#include <cstdint>
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
        uint32_t channel;
        uint32_t message_id;
        bool is_reply;
        std::vector<uint8_t> payload;
    };

    void write_packet(int fd, uint32_t channel, uint32_t message_id,
                      bool is_reply, const std::vector<uint8_t>& payload);

    Packet read_packet(int fd);

    // =============================================================================
    // CBOR Encode / Decode
    // =============================================================================

    inline std::vector<uint8_t> cbor_encode(const nlohmann::json& v) {
        return nlohmann::json::to_cbor(v);
    }

    inline nlohmann::json cbor_decode(const std::vector<uint8_t>& bytes) {
        return nlohmann::json::from_cbor(
            bytes, true, true, nlohmann::json::cbor_tag_handler_t::ignore);
    }

    inline nlohmann::json cbor_decode(const uint8_t* data, size_t len) {
        return nlohmann::json::from_cbor(
            data, data + len, true, true,
            nlohmann::json::cbor_tag_handler_t::ignore);
    }

} // namespace hegel::impl::protocol
