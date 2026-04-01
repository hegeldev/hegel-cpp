#include <connection.h>
#include <hegel/json.h>
#include <protocol.h>

#include "json_impl.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

using hegel::internal::json::ImplUtil;

// =============================================================================
// Connection and Stream Multiplexing
// =============================================================================
namespace hegel::impl {

    Connection::Connection(int fd) : fd_(fd) {}

    Connection::~Connection() { close(); }

    void Connection::close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    // =============================================================================
    // Stream Management
    // =============================================================================
    uint32_t Connection::create_stream() {
        uint32_t id = (next_stream_counter_ << 1) | 1;
        next_stream_counter_++;
        next_message_id_[id] = 0;
        return id;
    }

    uint32_t Connection::alloc_message_id(uint32_t stream) {
        return next_message_id_[stream]++;
    }

    // =============================================================================
    // Handshake
    // =============================================================================
    static constexpr const char* MIN_PROTOCOL_VERSION = "0.1";
    static constexpr const char* MAX_PROTOCOL_VERSION = "0.8";
    static constexpr const char* HANDSHAKE_STRING = "hegel_handshake_start";

    void Connection::handshake() {
        std::string hs(HANDSHAKE_STRING);
        std::vector<uint8_t> payload(hs.begin(), hs.end());
        uint32_t msg_id = alloc_message_id(0);
        protocol::write_packet(fd_, 0, msg_id, false, payload);

        // Wait for reply on stream 0
        auto packet = wait_for(0, true);
        std::string response(packet.payload.begin(), packet.payload.end());

        std::string prefix = "Hegel/";
        if (!response.starts_with(prefix)) {
            throw std::runtime_error("Bad handshake response: " + response);
        }

        std::string server_version = response.substr(prefix.size());
        double v = std::stod(server_version);
        double lo = std::stod(MIN_PROTOCOL_VERSION);
        double hi = std::stod(MAX_PROTOCOL_VERSION);
        if (v < lo || v > hi) {
            throw std::runtime_error(
                std::string("hegel-cpp supports protocol versions ") +
                MIN_PROTOCOL_VERSION + " through " + MAX_PROTOCOL_VERSION +
                ", but got server version " + server_version +
                ". Upgrading hegel-cpp or downgrading your hegel server "
                "might help.");
        }
    }

    // =============================================================================
    // Request / Reply
    // =============================================================================
    hegel::internal::json::json
    Connection::request(uint32_t stream,
                        const hegel::internal::json::json& msg) {
        auto payload = protocol::cbor_encode(ImplUtil::raw(msg));
        uint32_t msg_id = alloc_message_id(stream);
        protocol::write_packet(fd_, stream, msg_id, false, payload);

        auto packet = wait_for(stream, true);
        auto result = protocol::cbor_decode(packet.payload);
        return ImplUtil::create(result);
    }

    void Connection::write_reply(uint32_t stream, uint32_t message_id,
                                 const hegel::internal::json::json& msg) {
        auto payload = protocol::cbor_encode(ImplUtil::raw(msg));
        protocol::write_packet(fd_, stream, message_id, true, payload);
    }

    IncomingRequest Connection::recv_request(uint32_t stream) {
        auto packet = wait_for(stream, false);

        // Check for close-stream signal
        if (packet.payload.size() == 1 &&
            packet.payload[0] == protocol::CLOSE_PAYLOAD) {
            throw std::runtime_error("Stream closed by server");
        }

        auto decoded = protocol::cbor_decode(packet.payload);
        return IncomingRequest{packet.message_id, ImplUtil::create(decoded)};
    }

    void Connection::close_stream(uint32_t stream) {
        std::vector<uint8_t> payload = {protocol::CLOSE_PAYLOAD};
        protocol::write_packet(fd_, stream, protocol::CLOSE_MESSAGE_ID, false,
                               payload);
    }

    // =============================================================================
    // Packet Routing
    // =============================================================================
    protocol::Packet Connection::wait_for(uint32_t stream, bool want_reply) {
        // Check pending buffer first
        auto& queue = pending_[stream];
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (it->is_reply == want_reply) {
                auto packet = std::move(*it);
                queue.erase(it);
                return packet;
            }
        }

        // Read packets until we get the one we want
        while (true) {
            auto packet = protocol::read_packet(fd_);
            if (packet.stream == stream && packet.is_reply == want_reply) {
                return packet;
            }
            pending_[packet.stream].push_back(std::move(packet));
        }
    }

} // namespace hegel::impl
