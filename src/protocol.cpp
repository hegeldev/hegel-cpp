#include <protocol.h>

#include <crc32.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <hegel/settings.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h> // IWYU pragma: keep
#include <sys/types.h> // IWYU pragma: keep
#include <unistd.h>

namespace hegel::impl::protocol {
    static thread_local bool protocol_debug_ = false;

    void set_protocol_debug(bool enabled) { protocol_debug_ = enabled; }
    bool protocol_debug_enabled() { return protocol_debug_; }

    static bool is_protocol_debug_env() {
        const char* val = std::getenv("HEGEL_PROTOCOL_DEBUG");
        if (!val)
            return false;
        std::string v(val);
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "1" || v == "true";
    }

    void init_protocol_debug(Verbosity verbosity) {
        set_protocol_debug(verbosity == Verbosity::Debug ||
                           is_protocol_debug_env());
    }

    static void write_all(int fd, const uint8_t* data, size_t len) {
        size_t total = 0;
        while (total < len) {
            ssize_t n = ::write(fd, data + total, len - total);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                throw std::runtime_error("Write failed");
            }
            if (n == 0) {
                throw std::runtime_error("Write failed (zero bytes written)");
            }
            total += static_cast<size_t>(n);
        }
    }

    static void read_all(int fd, uint8_t* data, size_t len) {
        size_t total = 0;
        while (total < len) {
            ssize_t n = read(fd, data + total, len - total);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                throw std::runtime_error("Read failed");
            }
            if (n == 0) {
                throw std::runtime_error("Read failed (connection closed)");
            }
            total += static_cast<size_t>(n);
        }
    }

    void write_packet(int fd, uint32_t stream, uint32_t message_id,
                      bool is_reply, const std::vector<uint8_t>& payload) {
        uint32_t raw_msg_id = message_id | (is_reply ? REPLY_BIT : 0);
        uint32_t length = static_cast<uint32_t>(payload.size());

        // Build header with checksum zeroed for CRC calculation
        uint8_t header[HEADER_SIZE];
        // NOLINTNEXTLINE(misc-include-cleaner)
        uint32_t fields[5] = {htonl(MAGIC), 0, htonl(stream), htonl(raw_msg_id),
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

        if (protocol_debug_enabled()) {
            std::cerr << "SEND ch=" << stream << " msg=" << message_id
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
        uint32_t magic = ntohl(fields[0]); // NOLINT(misc-include-cleaner)
        uint32_t checksum = ntohl(fields[1]);
        uint32_t stream = ntohl(fields[2]);
        uint32_t raw_msg_id = ntohl(fields[3]);
        uint32_t length = ntohl(fields[4]);

        if (magic != MAGIC) {
            throw std::runtime_error("Bad magic in packet header");
        }

        // Read payload (cap at 64MB to prevent runaway allocations)
        if (length > 64 * 1024 * 1024) {
            throw std::runtime_error("Payload too large");
        }
        std::vector<uint8_t> payload(length);
        if (length > 0) {
            read_all(fd, payload.data(), length);
        }

        // Read terminator
        uint8_t term;
        read_all(fd, &term, 1);
        if (term != TERMINATOR) {
            throw std::runtime_error("Missing packet terminator");
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
            throw std::runtime_error("CRC32 mismatch");
        }

        bool is_reply = (raw_msg_id & REPLY_BIT) != 0;
        uint32_t message_id = raw_msg_id & ~REPLY_BIT;

        if (protocol_debug_enabled()) {
            std::cerr << "RECEIVE stream=" << stream
                      << " message_id=" << message_id << " reply=" << is_reply
                      << " len=" << length << "\n";
        }

        return Packet{stream, message_id, is_reply, std::move(payload)};
    }

} // namespace hegel::impl::protocol
