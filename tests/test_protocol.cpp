#include <gtest/gtest.h>

#include <crc32.h>
#include <protocol.h>

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

using hegel::impl::protocol::HEADER_SIZE;
using hegel::impl::protocol::MAGIC;
using hegel::impl::protocol::Packet;
using hegel::impl::protocol::read_packet;
using hegel::impl::protocol::REPLY_BIT;
using hegel::impl::protocol::TERMINATOR;
using hegel::impl::protocol::write_packet;

namespace {

    /// RAII wrapper around a pipe(2) pair.
    struct Pipe {
        int r = -1;
        int w = -1;
        Pipe() {
            int fds[2];
            if (::pipe(fds) != 0) {
                throw std::runtime_error("pipe failed");
            }
            r = fds[0];
            w = fds[1];
        }
        ~Pipe() {
            if (r >= 0)
                ::close(r);
            if (w >= 0)
                ::close(w);
        }
        Pipe(const Pipe&) = delete;
        Pipe& operator=(const Pipe&) = delete;
    };

    // Roundtrip `write_packet` -> `read_packet` through a pipe.
    // Because pipe buffers are small, the write happens on a thread to avoid
    // deadlocks on large payloads.
    Packet roundtrip(uint32_t stream, uint32_t msg_id, bool is_reply,
                     const std::vector<uint8_t>& payload) {
        Pipe p;
        std::thread writer([&] {
            write_packet(p.w, stream, msg_id, is_reply, payload);
            ::close(p.w);
            p.w = -1;
        });
        Packet got = read_packet(p.r);
        writer.join();
        return got;
    }

    // Serialize a packet into an in-memory buffer by teeing pipe output.
    std::vector<uint8_t> serialize_packet(uint32_t stream, uint32_t msg_id,
                                          bool is_reply,
                                          const std::vector<uint8_t>& payload) {
        Pipe p;
        std::thread writer([&] {
            write_packet(p.w, stream, msg_id, is_reply, payload);
            ::close(p.w);
            p.w = -1;
        });
        std::vector<uint8_t> buf;
        uint8_t tmp[1024];
        while (true) {
            ssize_t n = ::read(p.r, tmp, sizeof(tmp));
            if (n <= 0)
                break;
            buf.insert(buf.end(), tmp, tmp + n);
        }
        writer.join();
        return buf;
    }

    // Write `bytes` into a fresh pipe and read a packet out of the other end.
    // Used to test response to malformed packet bytes.
    Packet read_from_bytes(const std::vector<uint8_t>& bytes) {
        Pipe p;
        std::thread writer([&] {
            size_t off = 0;
            while (off < bytes.size()) {
                ssize_t n =
                    ::write(p.w, bytes.data() + off, bytes.size() - off);
                if (n <= 0)
                    break;
                off += static_cast<size_t>(n);
            }
            ::close(p.w);
            p.w = -1;
        });
        try {
            Packet got = read_packet(p.r);
            writer.join();
            return got;
        } catch (...) {
            writer.join();
            throw;
        }
    }

} // namespace

TEST(Protocol, RoundtripEmptyPayload) {
    Packet got = roundtrip(7, 42, false, {});
    EXPECT_EQ(got.stream, 7u);
    EXPECT_EQ(got.message_id, 42u);
    EXPECT_FALSE(got.is_reply);
    EXPECT_TRUE(got.payload.empty());
}

TEST(Protocol, RoundtripNonEmptyPayload) {
    std::vector<uint8_t> payload{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    Packet got = roundtrip(3, 100, false, payload);
    EXPECT_EQ(got.stream, 3u);
    EXPECT_EQ(got.message_id, 100u);
    EXPECT_FALSE(got.is_reply);
    EXPECT_EQ(got.payload, payload);
}

TEST(Protocol, RoundtripReplyBit) {
    std::vector<uint8_t> payload{0xAA, 0xBB};
    Packet got = roundtrip(1, 999, true, payload);
    EXPECT_EQ(got.stream, 1u);
    EXPECT_EQ(got.message_id, 999u);
    EXPECT_TRUE(got.is_reply);
    EXPECT_EQ(got.payload, payload);
}

TEST(Protocol, RoundtripLargePayload) {
    // 256 KB — comfortably larger than the macOS pipe buffer (~16 KB) to
    // exercise multi-chunk write/read with a writer thread.
    std::vector<uint8_t> payload(256 * 1024);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    Packet got = roundtrip(5, 1, false, payload);
    EXPECT_EQ(got.payload, payload);
}

TEST(Protocol, BadMagicRejected) {
    auto bytes = serialize_packet(1, 1, false, {0xDE, 0xAD});
    // Clobber magic.
    bytes[0] = 0x00;
    bytes[1] = 0x00;
    bytes[2] = 0x00;
    bytes[3] = 0x00;
    EXPECT_THROW(read_from_bytes(bytes), std::runtime_error);
}

TEST(Protocol, CrcMismatchDetected) {
    auto bytes = serialize_packet(1, 1, false, {0xDE, 0xAD, 0xBE, 0xEF});
    // Flip a payload byte (payload is after the 20-byte header).
    bytes[HEADER_SIZE] ^= 0xFF;
    EXPECT_THROW(read_from_bytes(bytes), std::runtime_error);
}

TEST(Protocol, MissingTerminatorDetected) {
    auto bytes = serialize_packet(1, 1, false, {0x01});
    // Clobber terminator (last byte).
    bytes.back() = 0x00;
    EXPECT_THROW(read_from_bytes(bytes), std::runtime_error);
}

TEST(Protocol, OversizedLengthRejected) {
    // Build a valid-looking header with length > 64 MB and a valid CRC so
    // that the length check is the first thing to fail.
    std::vector<uint8_t> header(HEADER_SIZE, 0);
    uint32_t fields[5];
    fields[0] = htonl(MAGIC);
    fields[1] = 0;                                  // checksum placeholder
    fields[2] = htonl(uint32_t{1});                 // stream
    fields[3] = htonl(uint32_t{1});                 // message_id
    fields[4] = htonl(uint32_t{100 * 1024 * 1024}); // length (100 MB)
    std::memcpy(header.data(), fields, HEADER_SIZE);

    // Compute CRC over header-with-checksum-zeroed so this isn't also a
    // CRC failure. (The 64MB cap should trigger before we read payload.)
    uint32_t ck = hegel::impl::crc32(header.data(), HEADER_SIZE);
    uint32_t net_ck = htonl(ck);
    std::memcpy(header.data() + 4, &net_ck, 4);

    // We can't actually deliver 100 MB of payload — but we shouldn't need
    // to: the length check rejects before reading any payload bytes.
    EXPECT_THROW(read_from_bytes(header), std::runtime_error);
}

TEST(Protocol, TruncatedHeaderFails) {
    // Write only 10 bytes of a 20-byte header and close the pipe.
    std::vector<uint8_t> bytes(10, 0);
    EXPECT_THROW(read_from_bytes(bytes), std::runtime_error);
}

TEST(Protocol, TruncatedPayloadFails) {
    auto full = serialize_packet(1, 1, false, std::vector<uint8_t>(100, 0xAA));
    // Truncate to header + half the payload.
    std::vector<uint8_t> truncated(full.begin(),
                                   full.begin() + HEADER_SIZE + 50);
    EXPECT_THROW(read_from_bytes(truncated), std::runtime_error);
}

TEST(Protocol, ReplyBitBoundary) {
    // message_id = REPLY_BIT - 1 should roundtrip cleanly.
    uint32_t high_msg = REPLY_BIT - 1;
    Packet got = roundtrip(1, high_msg, false, {});
    EXPECT_EQ(got.message_id, high_msg);
    EXPECT_FALSE(got.is_reply);
}
