#pragma once

#include <cstdint>
#include <hegel/cbor.h>
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

void write_packet(int fd, uint32_t channel, uint32_t message_id, bool is_reply,
                  const std::vector<uint8_t>& payload);

Packet read_packet(int fd);

} // namespace hegel::impl::protocol
