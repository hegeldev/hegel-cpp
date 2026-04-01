#include <connection.h>
#include <protocol.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

// =============================================================================
// Connection and Channel Multiplexing
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
    // Channel Management
    // =============================================================================
    uint32_t Connection::create_channel() {
        uint32_t id = (next_channel_counter_ << 1) | 1;
        next_channel_counter_++;
        next_message_id_[id] = 0;
        return id;
    }

    uint32_t Connection::alloc_message_id(uint32_t channel) {
        return next_message_id_[channel]++;
    }

    // =============================================================================
    // Handshake
    // =============================================================================
    static constexpr const char* MIN_PROTOCOL_VERSION = "0.1";
    static constexpr const char* MAX_PROTOCOL_VERSION = "0.4";
    static constexpr const char* HANDSHAKE_STRING = "hegel_handshake_start";

    void Connection::handshake() {
        std::string hs(HANDSHAKE_STRING);
        std::vector<uint8_t> payload(hs.begin(), hs.end());
        uint32_t msg_id = alloc_message_id(0);
        protocol::write_packet(fd_, 0, msg_id, false, payload);

        // Wait for reply on channel 0
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
                ", but the connected server is using protocol version " +
                server_version +
                ". Upgrading hegel-cpp or downgrading hegel-core "
                "might help.");
        }
    }

    // =============================================================================
    // Request / Reply
    // =============================================================================
    nlohmann::json Connection::request(uint32_t channel,
                                       const nlohmann::json& msg) {
        auto payload = protocol::cbor_encode(msg);
        uint32_t msg_id = alloc_message_id(channel);
        protocol::write_packet(fd_, channel, msg_id, false, payload);

        auto packet = wait_for(channel, true);
        return protocol::cbor_decode(packet.payload);
    }

    void Connection::write_reply(uint32_t channel, uint32_t message_id,
                                 const nlohmann::json& msg) {
        auto payload = protocol::cbor_encode(msg);
        protocol::write_packet(fd_, channel, message_id, true, payload);
    }

    IncomingRequest Connection::recv_request(uint32_t channel) {
        auto packet = wait_for(channel, false);

        // Check for close-channel signal
        if (packet.payload.size() == 1 &&
            packet.payload[0] == protocol::CLOSE_PAYLOAD) {
            throw std::runtime_error("Channel closed by server");
        }

        return IncomingRequest{packet.message_id,
                               protocol::cbor_decode(packet.payload)};
    }

    void Connection::close_channel(uint32_t channel) {
        std::vector<uint8_t> payload = {protocol::CLOSE_PAYLOAD};
        protocol::write_packet(fd_, channel, protocol::CLOSE_MESSAGE_ID, false,
                               payload);
    }

    // =============================================================================
    // Packet Routing
    // =============================================================================
    protocol::Packet Connection::wait_for(uint32_t channel, bool want_reply) {
        // Check pending buffer first
        auto& queue = pending_[channel];
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
            if (packet.channel == channel && packet.is_reply == want_reply) {
                return packet;
            }
            pending_[packet.channel].push_back(std::move(packet));
        }
    }

} // namespace hegel::impl
