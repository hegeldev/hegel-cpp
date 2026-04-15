// test_protocol.cpp - Protocol and JSON wrapper unit tests
// Exercises CRC32, CBOR encode/decode, JSON wrapper methods,
// packet serialization, and Connection unit tests without requiring a live
// server.

#include <gtest/gtest.h>

#include <hegel/internal.h>
#include <hegel/json.h>
#include <hegel/options.h>

#include "../src/connection.h"
#include "../src/data.h"
#include "../src/data_source.h"
#include "../src/json_impl.h"
#include "../src/protocol.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using hegel::internal::json::ImplUtil;
using hegel::internal::json::json;
using hegel::internal::json::json_raw_ref;

// =============================================================================
// CRC32 tests
// =============================================================================

TEST(CRC32, EmptyInput) {
    uint32_t result = hegel::impl::protocol::crc32(nullptr, 0);
    ASSERT_EQ(result, 0x00000000u);
}

TEST(CRC32, KnownVector) {
    // "123456789" should produce CRC32 = 0xCBF43926
    const uint8_t data[] = "123456789";
    uint32_t result = hegel::impl::protocol::crc32(data, 9);
    ASSERT_EQ(result, 0xCBF43926u);
}

TEST(CRC32, SingleByte) {
    uint8_t data[] = {0x00};
    uint32_t result = hegel::impl::protocol::crc32(data, 1);
    ASSERT_NE(result, 0u);
}

// =============================================================================
// CBOR encode/decode tests
// =============================================================================

TEST(CBOR, RoundTripInteger) {
    nlohmann::json original = 42;
    auto encoded = hegel::impl::protocol::cbor_encode(original);
    auto decoded = hegel::impl::protocol::cbor_decode(encoded);
    ASSERT_EQ(decoded, original);
}

TEST(CBOR, RoundTripString) {
    nlohmann::json original = "hello world";
    auto encoded = hegel::impl::protocol::cbor_encode(original);
    auto decoded = hegel::impl::protocol::cbor_decode(encoded);
    ASSERT_EQ(decoded, original);
}

TEST(CBOR, RoundTripObject) {
    nlohmann::json original = {{"key", "value"}, {"number", 123}};
    auto encoded = hegel::impl::protocol::cbor_encode(original);
    auto decoded = hegel::impl::protocol::cbor_decode(encoded);
    ASSERT_EQ(decoded, original);
}

TEST(CBOR, RoundTripArray) {
    nlohmann::json original = {1, 2, 3, "four"};
    auto encoded = hegel::impl::protocol::cbor_encode(original);
    auto decoded = hegel::impl::protocol::cbor_decode(encoded);
    ASSERT_EQ(decoded, original);
}

TEST(CBOR, DecodeFromPointer) {
    nlohmann::json original = {{"test", true}};
    auto encoded = hegel::impl::protocol::cbor_encode(original);
    auto decoded =
        hegel::impl::protocol::cbor_decode(encoded.data(), encoded.size());
    ASSERT_EQ(decoded, original);
}

TEST(CBOR, TaggedStringConversion) {
    // Create a CBOR binary with tag 91 (hegel string)
    nlohmann::json tagged =
        nlohmann::json::binary({0x68, 0x65, 0x6c, 0x6c, 0x6f}, 91);
    hegel::impl::protocol::convert_tagged_strings(tagged);
    ASSERT_TRUE(tagged.is_string());
    ASSERT_EQ(tagged.get<std::string>(), "hello");
}

TEST(CBOR, TaggedStringInArray) {
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back(nlohmann::json::binary({0x68, 0x69}, 91));
    arr.push_back(42);
    hegel::impl::protocol::convert_tagged_strings(arr);
    ASSERT_TRUE(arr[0].is_string());
    ASSERT_EQ(arr[0].get<std::string>(), "hi");
    ASSERT_EQ(arr[1], 42);
}

TEST(CBOR, TaggedStringInObject) {
    nlohmann::json obj = {{"key", nlohmann::json::binary({0x6f, 0x6b}, 91)}};
    hegel::impl::protocol::convert_tagged_strings(obj);
    ASSERT_EQ(obj["key"].get<std::string>(), "ok");
}

// =============================================================================
// Packet read/write round-trip tests (using socket pairs)
// =============================================================================

TEST(Packet, WriteReadRoundTrip) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    hegel::impl::protocol::write_packet(fds[0], 1, 5, false, payload);

    auto packet = hegel::impl::protocol::read_packet(fds[1]);
    ASSERT_EQ(packet.stream, 1u);
    ASSERT_EQ(packet.message_id, 5u);
    ASSERT_FALSE(packet.is_reply);
    ASSERT_EQ(packet.payload, payload);

    close(fds[0]);
    close(fds[1]);
}

TEST(Packet, WriteReadReplyBit) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<uint8_t> payload = {0xAB};
    hegel::impl::protocol::write_packet(fds[0], 3, 7, true, payload);

    auto packet = hegel::impl::protocol::read_packet(fds[1]);
    ASSERT_EQ(packet.stream, 3u);
    ASSERT_EQ(packet.message_id, 7u);
    ASSERT_TRUE(packet.is_reply);
    ASSERT_EQ(packet.payload, payload);

    close(fds[0]);
    close(fds[1]);
}

TEST(Packet, EmptyPayload) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<uint8_t> payload = {};
    hegel::impl::protocol::write_packet(fds[0], 0, 0, false, payload);

    auto packet = hegel::impl::protocol::read_packet(fds[1]);
    ASSERT_EQ(packet.stream, 0u);
    ASSERT_TRUE(packet.payload.empty());

    close(fds[0]);
    close(fds[1]);
}

TEST(Packet, LargePayload) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<uint8_t> payload(1024, 0x42);
    hegel::impl::protocol::write_packet(fds[0], 2, 10, false, payload);

    auto packet = hegel::impl::protocol::read_packet(fds[1]);
    ASSERT_EQ(packet.payload.size(), 1024u);
    ASSERT_EQ(packet.payload[0], 0x42);
    ASSERT_EQ(packet.payload[1023], 0x42);

    close(fds[0]);
    close(fds[1]);
}

// =============================================================================
// JSON wrapper tests
// =============================================================================

TEST(JsonWrapper, ConstructFromTypes) {
    json j_str("hello");
    json j_int(42);
    json j_int64(int64_t{100});
    json j_uint(uint32_t{99});
    json j_uint64(uint64_t{200});
    json j_bool(true);
    json j_double(3.14);
    json j_null(nullptr);

    ASSERT_EQ(j_str.dump(), "\"hello\"");
    ASSERT_TRUE(j_null.dump().find("null") != std::string::npos);
}

TEST(JsonWrapper, ObjectAccess) {
    json obj = {{"name", "test"}, {"value", 42}};
    ASSERT_TRUE(obj.contains("name"));
    ASSERT_FALSE(obj.contains("missing"));
    ASSERT_EQ(obj.value("name", ""), "test");
    ASSERT_EQ(obj.value("missing", "default"), "default");
}

TEST(JsonWrapper, Assignment) {
    json a = {{"x", 1}};
    json b = {{"y", 2}};
    a = b;
    ASSERT_TRUE(a.contains("y"));
}

TEST(JsonWrapper, SelfAssignment) {
    json a = {{"x", 1}};
    a = a;
    ASSERT_TRUE(a.contains("x"));
}

TEST(JsonWrapper, ArrayConstruction) {
    json arr = json::array({json(1), json(2), json(3)});
    auto raw = ImplUtil::raw(arr);
    ASSERT_TRUE(raw.is_array());
    ASSERT_EQ(raw.size(), 3u);
}

TEST(JsonWrapper, PushBack) {
    json arr = json::array({});
    arr.push_back(json(1));
    arr.push_back(json(2));
    auto raw = ImplUtil::raw(arr);
    ASSERT_EQ(raw.size(), 2u);
}

TEST(JsonWrapper, PushBackString) {
    json arr = json::array({});
    arr.push_back(std::string("hello"));
    auto raw = ImplUtil::raw(arr);
    ASSERT_EQ(raw.size(), 1u);
    ASSERT_EQ(raw[0].get<std::string>(), "hello");
}

TEST(JsonWrapper, Parse) {
    json j = json::parse("{\"key\": 42}");
    ASSERT_TRUE(j.contains("key"));
}

TEST(JsonWrapper, ValueUint32) {
    json obj = {{"count", 5}};
    uint32_t val = obj.value("count", uint32_t{0});
    ASSERT_EQ(val, 5u);
    uint32_t def = obj.value("missing", uint32_t{99});
    ASSERT_EQ(def, 99u);
}

// =============================================================================
// json_raw_ref type checking and accessors
// =============================================================================

TEST(JsonRawRef, TypeChecking) {
    json obj = {{"str", "hello"},
                {"num", 42},
                {"flag", true},
                {"nil", nullptr},
                {"pi", 3.14}};

    auto str_ref = obj["str"];
    ASSERT_TRUE(str_ref.is_string());
    ASSERT_FALSE(str_ref.is_null());
    ASSERT_FALSE(str_ref.is_boolean());
    ASSERT_FALSE(str_ref.is_number());

    auto num_ref = obj["num"];
    ASSERT_TRUE(num_ref.is_number());
    ASSERT_TRUE(num_ref.is_number_integer());
    ASSERT_FALSE(num_ref.is_number_unsigned());
    ASSERT_FALSE(num_ref.is_string());

    auto bool_ref = obj["flag"];
    ASSERT_TRUE(bool_ref.is_boolean());
    ASSERT_FALSE(bool_ref.is_number());

    auto null_ref = obj["nil"];
    ASSERT_TRUE(null_ref.is_null());

    auto dbl_ref = obj["pi"];
    ASSERT_TRUE(dbl_ref.is_number());
}

TEST(JsonRawRef, GetValues) {
    json obj = {{"str", "hello"}, {"num", 42}, {"flag", true}, {"pi", 3.14}};

    ASSERT_EQ(obj["str"].get_string(), "hello");
    ASSERT_EQ(obj["num"].get_int64_t(), 42);
    ASSERT_EQ(obj["flag"].get_bool(), true);
    ASSERT_DOUBLE_EQ(obj["pi"].get_double(), 3.14);
}

TEST(JsonRawRef, UnsignedAccess) {
    json obj = {{"val", 99}};
    ASSERT_EQ(obj["val"].get_uint32_t(), 99u);
    ASSERT_EQ(obj["val"].get_uint64_t(), 99u);
}

TEST(JsonRawRef, ArrayAccess) {
    json arr_holder = {{"arr", json::array({json(10), json(20), json(30)})}};
    auto arr_ref = arr_holder["arr"];
    ASSERT_TRUE(arr_ref.is_array());
    ASSERT_EQ(arr_ref.size(), 3u);
    ASSERT_EQ(arr_ref[0].get_int64_t(), 10);
    ASSERT_EQ(arr_ref[2].get_int64_t(), 30);
}

TEST(JsonRawRef, ObjectAccess) {
    json obj = {{"nested", json({{"a", 1}, {"b", 2}})}};
    auto ref = obj["nested"];
    ASSERT_TRUE(ref.is_object());
}

TEST(JsonRawRef, Iterate) {
    json arr_holder = {{"arr", json::array({json(1), json(2), json(3)})}};
    auto items = arr_holder["arr"].iterate();
    ASSERT_EQ(items.size(), 3u);
    ASSERT_EQ(items[0].get_int64_t(), 1);
    ASSERT_EQ(items[2].get_int64_t(), 3);
}

TEST(JsonRawRef, Items) {
    json obj = {{"a", 1}, {"b", 2}};
    auto raw = ImplUtil::raw(obj);
    auto wrapper = ImplUtil::create(raw);
    // Use items via the raw_ref
    auto ref = wrapper["a"];
    ASSERT_EQ(ref.get_int64_t(), 1);
}

TEST(JsonRawRef, Find) {
    json obj = {{"key", "value"}};
    auto raw_ref = obj["key"];
    // find is on json_raw_ref for sub-objects
    // Use an object ref to test find
    nlohmann::json raw_obj = {{"nested", {{"x", 42}}}};
    auto wrapper = ImplUtil::create(raw_obj);
    auto nested = wrapper["nested"];
    auto found = nested.find("x");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->get_int64_t(), 42);
    auto missing = nested.find("y");
    ASSERT_FALSE(missing.has_value());
}

TEST(JsonRawRef, ItemsIteration) {
    nlohmann::json raw_obj = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto wrapper = ImplUtil::create(raw_obj);
    // Create a json_raw_ref to the whole object by wrapping
    nlohmann::json outer = {{"obj", raw_obj}};
    auto outer_wrapper = ImplUtil::create(outer);
    auto obj_ref = outer_wrapper["obj"];
    auto kv_pairs = obj_ref.items();
    ASSERT_EQ(kv_pairs.size(), 3u);
    // items returns key-value pairs
    bool found_a = false;
    for (const auto& [key, val] : kv_pairs) {
        if (key == "a") {
            ASSERT_EQ(val.get_int64_t(), 1);
            found_a = true;
        }
    }
    ASSERT_TRUE(found_a);
}

// =============================================================================
// Protocol debug flag
// =============================================================================

TEST(ProtocolDebug, SetAndGet) {
    bool original = hegel::impl::protocol::protocol_debug_enabled();
    hegel::impl::protocol::set_protocol_debug(true);
    ASSERT_TRUE(hegel::impl::protocol::protocol_debug_enabled());
    hegel::impl::protocol::set_protocol_debug(false);
    ASSERT_FALSE(hegel::impl::protocol::protocol_debug_enabled());
    // Restore
    hegel::impl::protocol::set_protocol_debug(original);
}

TEST(ProtocolDebug, InitFromVerbosity) {
    hegel::impl::protocol::init_protocol_debug(
        hegel::options::Verbosity::Debug);
    ASSERT_TRUE(hegel::impl::protocol::protocol_debug_enabled());
    hegel::impl::protocol::init_protocol_debug(
        hegel::options::Verbosity::Normal);
    // Without env var set, normal should not enable debug
}

TEST(ProtocolDebug, WriteWithDebugEnabled) {
    // Enable debug, write a packet, and verify it still works
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    hegel::impl::protocol::set_protocol_debug(true);

    std::vector<uint8_t> payload = {0x01};
    hegel::impl::protocol::write_packet(fds[0], 0, 0, false, payload);
    auto packet = hegel::impl::protocol::read_packet(fds[1]);
    ASSERT_EQ(packet.payload, payload);

    hegel::impl::protocol::set_protocol_debug(false);
    close(fds[0]);
    close(fds[1]);
}

// =============================================================================
// Protocol error paths
// =============================================================================

TEST(PacketErrors, ReadFromClosedFd) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    close(fds[0]); // Close writer end
    ASSERT_THROW(hegel::impl::protocol::read_packet(fds[1]),
                 std::runtime_error);
    close(fds[1]);
}

TEST(PacketErrors, CorruptedMagic) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    // Write a header with bad magic
    uint8_t header[20] = {};
    uint32_t bad_magic = htonl(0xDEADBEEFu);
    memcpy(header, &bad_magic, 4);
    // Fill rest to avoid read stall
    write(fds[0], header, 20);
    close(fds[0]);

    ASSERT_THROW(hegel::impl::protocol::read_packet(fds[1]),
                 std::runtime_error);
    close(fds[1]);
}

TEST(PacketErrors, CorruptedCRC) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    // Write a valid packet then corrupt the CRC
    std::vector<uint8_t> payload = {0x42};
    hegel::impl::protocol::write_packet(fds[0], 0, 0, false, payload);

    // Read the raw bytes, corrupt CRC, rewrite
    // Simpler: just write a valid-looking header with wrong CRC
    // Actually, the packet is already on fds[1]. Let's just read
    // the valid packet and test the corrupted one separately.
    auto valid = hegel::impl::protocol::read_packet(fds[1]);
    ASSERT_EQ(valid.payload, payload); // sanity check

    // Now write a header with correct magic but wrong CRC
    uint8_t header[20];
    uint32_t fields[5] = {htonl(0x4845474Cu), htonl(0xBADC0000u), 0, 0, 0};
    memcpy(header, fields, 20);
    uint8_t term = 0x0A;
    write(fds[0], header, 20);
    write(fds[0], &term, 1);

    ASSERT_THROW(hegel::impl::protocol::read_packet(fds[1]),
                 std::runtime_error);

    close(fds[0]);
    close(fds[1]);
}

// =============================================================================
// Protocol error paths (payload too large, missing terminator)
// =============================================================================

TEST(PacketErrors, PayloadTooLarge) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    // Write a header where length > 64MB
    uint8_t header[20];
    uint32_t magic = htonl(0x4845474Cu);
    uint32_t crc = htonl(0u);
    uint32_t stream = htonl(0u);
    uint32_t msg_id = htonl(0u);
    uint32_t length = htonl(64 * 1024 * 1024 + 1); // just over limit
    memcpy(header + 0, &magic, 4);
    memcpy(header + 4, &crc, 4);
    memcpy(header + 8, &stream, 4);
    memcpy(header + 12, &msg_id, 4);
    memcpy(header + 16, &length, 4);
    write(fds[0], header, 20);
    close(fds[0]);

    ASSERT_THROW(hegel::impl::protocol::read_packet(fds[1]),
                 std::runtime_error);
    close(fds[1]);
}

TEST(PacketErrors, MissingTerminator) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    // Write a valid header (correct magic, zero-byte payload) followed by
    // a wrong terminator byte. The terminator check fires before CRC check,
    // so the CRC in the header doesn't need to be correct.
    uint8_t header[20];
    uint32_t magic = htonl(0x4845474Cu);
    memcpy(header + 0, &magic, 4);
    memset(header + 4, 0, 16); // crc=0, stream=0, msg_id=0, length=0
    write(fds[0], header, 20);
    uint8_t bad_term = 0xFF; // wrong terminator (not 0x0A)
    write(fds[0], &bad_term, 1);
    close(fds[0]);

    ASSERT_THROW(hegel::impl::protocol::read_packet(fds[1]),
                 std::runtime_error);
    close(fds[1]);
}

// =============================================================================
// Protocol debug env var (HEGEL_PROTOCOL_DEBUG)
// =============================================================================

TEST(ProtocolDebug, EnvVarEnabled) {
    // Save original state
    bool original = hegel::impl::protocol::protocol_debug_enabled();

    setenv("HEGEL_PROTOCOL_DEBUG", "1", 1);
    hegel::impl::protocol::init_protocol_debug(
        hegel::options::Verbosity::Normal);
    ASSERT_TRUE(hegel::impl::protocol::protocol_debug_enabled());

    unsetenv("HEGEL_PROTOCOL_DEBUG");
    hegel::impl::protocol::init_protocol_debug(
        hegel::options::Verbosity::Normal);
    ASSERT_FALSE(hegel::impl::protocol::protocol_debug_enabled());

    // Restore
    hegel::impl::protocol::set_protocol_debug(original);
}

// =============================================================================
// JSON wrapper: push_back(const json&) and get_binary()
// =============================================================================

TEST(JsonWrapper, PushBackConstRef) {
    json arr = json::array({});
    const json elem(99);
    arr.push_back(elem); // calls push_back(const json&)
    auto raw = ImplUtil::raw(arr);
    ASSERT_EQ(raw.size(), 1u);
    ASSERT_EQ(raw[0].get<int>(), 99);
}

TEST(JsonWrapper, GetBinary) {
    // Create a json containing binary data via nlohmann
    nlohmann::json bin_json = nlohmann::json::binary({0x01, 0x02, 0x03});
    auto j = ImplUtil::create(bin_json);
    auto& bytes = j.get_binary();
    ASSERT_EQ(bytes.size(), 3u);
    ASSERT_EQ(bytes[0], 0x01);
    ASSERT_EQ(bytes[2], 0x03);
}

// =============================================================================
// Connection unit tests using socketpairs
// =============================================================================

namespace {

    // Write a fake server handshake reply to server_fd.
    // The handshake flow: client sends request (stream=0, is_reply=false),
    // then waits for reply (stream=0, is_reply=true).
    // We pre-write the reply so it's ready when the client reads.
    void send_handshake_reply(int server_fd, const std::string& response) {
        std::vector<uint8_t> payload(response.begin(), response.end());
        hegel::impl::protocol::write_packet(server_fd, 0, 0, true, payload);
    }

} // namespace

TEST(Connection, HandshakeBadResponse) {
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    send_handshake_reply(sv[1], "NotHegel/anything");
    {
        hegel::impl::Connection conn(sv[0], sv[0]);
        ASSERT_THROW(conn.handshake(), std::runtime_error);
    }
    close(sv[1]);
}

TEST(Connection, HandshakeMajorVersionTooHigh) {
    // "Hegel/1.0" — major version 1 > 0, out of supported range.
    // Exercises compare_versions line where a_major != b_major (line 84).
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    send_handshake_reply(sv[1], "Hegel/1.0");
    {
        hegel::impl::Connection conn(sv[0], sv[0]);
        ASSERT_THROW(conn.handshake(), std::runtime_error);
    }
    close(sv[1]);
}

TEST(Connection, HandshakeMinorVersionTooLow) {
    // "Hegel/0.9" — minor version 9 < 10, out of supported range.
    // Exercises compare_versions line where a_minor != b_minor (line 86).
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    send_handshake_reply(sv[1], "Hegel/0.9");
    {
        hegel::impl::Connection conn(sv[0], sv[0]);
        ASSERT_THROW(conn.handshake(), std::runtime_error);
    }
    close(sv[1]);
}

TEST(Connection, RecvRequestClosedStream) {
    // recv_request should throw when payload is CLOSE_PAYLOAD.
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    // Pre-write a close-stream signal: stream=1, is_reply=false (not a reply)
    std::vector<uint8_t> close_payload = {hegel::impl::protocol::CLOSE_PAYLOAD};
    hegel::impl::protocol::write_packet(sv[1], 1, 0, false, close_payload);

    {
        hegel::impl::Connection conn(sv[0], sv[0]);
        ASSERT_THROW(conn.recv_request(1), std::runtime_error);
    }
    close(sv[1]);
}

TEST(Connection, CloseStream) {
    // close_stream should write a CLOSE_PAYLOAD packet.
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    {
        hegel::impl::Connection conn(sv[0], sv[0]);
        conn.close_stream(3);
    }
    // Read the packet from server side and verify it's a close signal
    auto pkt = hegel::impl::protocol::read_packet(sv[1]);
    ASSERT_EQ(pkt.stream, 3u);
    ASSERT_EQ(pkt.payload.size(), 1u);
    ASSERT_EQ(pkt.payload[0], hegel::impl::protocol::CLOSE_PAYLOAD);

    close(sv[1]);
}

// =============================================================================
// communicate_with_core: debug logging and generic error paths
// =============================================================================

namespace {

    // Write a CBOR-encoded JSON value as a fake server reply on the given
    // stream.
    void send_cbor_reply(int server_fd, uint32_t stream,
                         const nlohmann::json& value) {
        auto payload = hegel::impl::protocol::cbor_encode(value);
        hegel::impl::protocol::write_packet(server_fd, stream, 0, true,
                                            payload);
    }

} // namespace

TEST(CommunicateWithCore, DebugLoggingEnabled) {
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    // Pre-write a successful response: {"result": 42}
    send_cbor_reply(sv[1], 1, {{"result", 42}});

    hegel::impl::Connection conn(sv[0], sv[0]);
    hegel::impl::ConnectionDataSource source(&conn, 1);
    hegel::impl::data::TestCaseData data{
        .source = &source,
        .is_last_run = false,
        .test_aborted = false,
        .verbosity = hegel::options::Verbosity::Normal,
    };
    hegel::impl::data::set(&data);

    // Enable debug logging so the REQUEST/RESPONSE paths execute
    hegel::impl::protocol::set_protocol_debug(true);

    json schema = {{"type", "integer"}};
    auto result = hegel::internal::communicate_with_core(schema, &data);

    hegel::impl::protocol::set_protocol_debug(false);
    hegel::impl::data::clear();
    close(sv[1]);

    ASSERT_TRUE(result.contains("result"));
}

TEST(CommunicateWithCore, GenericErrorNoType) {
    // Server returns {"error": "something went wrong"} without a "type" field.
    // This exercises the generic throw path (lines 224-227 of connection.cpp).
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    send_cbor_reply(sv[1], 1, {{"error", "something went wrong"}});

    hegel::impl::Connection conn(sv[0], sv[0]);
    hegel::impl::ConnectionDataSource source(&conn, 1);
    hegel::impl::data::TestCaseData data{
        .source = &source,
        .is_last_run = false,
        .test_aborted = false,
        .verbosity = hegel::options::Verbosity::Normal,
    };
    hegel::impl::data::set(&data);

    json schema = {{"type", "integer"}};
    bool threw = false;
    try {
        hegel::internal::communicate_with_core(schema, &data);
    } catch (const std::runtime_error&) {
        threw = true;
    }

    hegel::impl::data::clear();
    close(sv[1]);

    ASSERT_TRUE(threw);
}

// =============================================================================
// internal::note() with is_last_run = true
// =============================================================================

TEST(InternalNote, PrintsOnLastRun) {
    hegel::impl::data::TestCaseData data{
        .source = nullptr,   // no I/O needed
        .is_last_run = true, // triggers the std::cerr path
        .test_aborted = false,
        .verbosity = hegel::options::Verbosity::Normal,
    };
    hegel::impl::data::set(&data);
    // note() prints to std::cerr when is_last_run; no exception expected
    ASSERT_NO_THROW(hegel::internal::note("test note message"));
    hegel::impl::data::clear();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
