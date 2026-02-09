#include <protocol.h>

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

// =============================================================================
// Binary Packet Protocol
// =============================================================================
namespace hegel::impl::protocol {

// =============================================================================
// CRC32 (table-driven, polynomial 0xEDB88320)
// =============================================================================
static constexpr uint32_t make_crc_entry(uint32_t c) {
    for (int k = 0; k < 8; ++k) {
        if (c & 1)
            c = 0xEDB88320 ^ (c >> 1);
        else
            c >>= 1;
    }
    return c;
}

static constexpr auto make_crc_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        table[i] = make_crc_entry(i);
    }
    return table;
}

static constexpr auto CRC_TABLE = make_crc_table();

uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = CRC_TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// Low-level I/O
// =============================================================================
static void write_all(int fd, const uint8_t* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, data + total, len - total);
        if (n <= 0) {
            throw std::runtime_error("hegel: write failed");
        }
        total += static_cast<size_t>(n);
    }
}

static void read_all(int fd, uint8_t* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, data + total, len - total);
        if (n <= 0) {
            throw std::runtime_error("hegel: read failed (connection closed)");
        }
        total += static_cast<size_t>(n);
    }
}

// =============================================================================
// Packet I/O
// =============================================================================
void write_packet(int fd, uint32_t channel, uint32_t message_id, bool is_reply,
                  const std::vector<uint8_t>& payload) {
    uint32_t raw_msg_id = message_id | (is_reply ? REPLY_BIT : 0);
    uint32_t length = static_cast<uint32_t>(payload.size());

    // Build header with checksum zeroed for CRC calculation
    uint8_t header[HEADER_SIZE];
    uint32_t fields[5] = {htonl(MAGIC), 0, htonl(channel), htonl(raw_msg_id),
                          htonl(length)};
    std::memcpy(header, fields, HEADER_SIZE);

    // Compute CRC over header (checksum zeroed) + payload
    std::vector<uint8_t> crc_buf(HEADER_SIZE + payload.size());
    std::memcpy(crc_buf.data(), header, HEADER_SIZE);
    if (!payload.empty()) {
        std::memcpy(crc_buf.data() + HEADER_SIZE, payload.data(),
                    payload.size());
    }
    uint32_t checksum = crc32(crc_buf.data(), crc_buf.size());

    // Fill in checksum
    uint32_t net_checksum = htonl(checksum);
    std::memcpy(header + 4, &net_checksum, 4);

    if (std::getenv("HEGEL_DEBUG")) {
        std::cerr << "SEND ch=" << channel << " msg=" << message_id
                  << " reply=" << is_reply << " len=" << length << "\n";
    }

    write_all(fd, header, HEADER_SIZE);
    if (!payload.empty()) {
        write_all(fd, payload.data(), payload.size());
    }
    uint8_t term = TERMINATOR;
    write_all(fd, &term, 1);
}

Packet read_packet(int fd) {
    // Read header
    uint8_t header[HEADER_SIZE];
    read_all(fd, header, HEADER_SIZE);

    uint32_t fields[5];
    std::memcpy(fields, header, HEADER_SIZE);
    uint32_t magic = ntohl(fields[0]);
    uint32_t checksum = ntohl(fields[1]);
    uint32_t channel = ntohl(fields[2]);
    uint32_t raw_msg_id = ntohl(fields[3]);
    uint32_t length = ntohl(fields[4]);

    if (magic != MAGIC) {
        throw std::runtime_error("hegel: bad magic in packet header");
    }

    // Read payload (cap at 64MB to prevent runaway allocations)
    if (length > 64 * 1024 * 1024) {
        throw std::runtime_error("hegel: payload too large");
    }
    std::vector<uint8_t> payload(length);
    if (length > 0) {
        read_all(fd, payload.data(), length);
    }

    // Read terminator
    uint8_t term;
    read_all(fd, &term, 1);
    if (term != TERMINATOR) {
        throw std::runtime_error("hegel: missing packet terminator");
    }

    // Verify CRC: zero checksum field, compute over header + payload
    uint8_t verify_header[HEADER_SIZE];
    std::memcpy(verify_header, header, HEADER_SIZE);
    std::memset(verify_header + 4, 0, 4);

    std::vector<uint8_t> crc_buf(HEADER_SIZE + length);
    std::memcpy(crc_buf.data(), verify_header, HEADER_SIZE);
    if (length > 0) {
        std::memcpy(crc_buf.data() + HEADER_SIZE, payload.data(), length);
    }
    uint32_t computed = crc32(crc_buf.data(), crc_buf.size());
    if (computed != checksum) {
        throw std::runtime_error("hegel: CRC32 mismatch");
    }

    bool is_reply = (raw_msg_id & REPLY_BIT) != 0;
    uint32_t message_id = raw_msg_id & ~REPLY_BIT;

    if (std::getenv("HEGEL_DEBUG")) {
        std::cerr << "RECV ch=" << channel << " msg=" << message_id
                  << " reply=" << is_reply << " len=" << length << "\n";
    }

    return Packet{channel, message_id, is_reply, std::move(payload)};
}

} // namespace hegel::impl::protocol
