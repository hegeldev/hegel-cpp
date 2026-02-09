#include <connection.h>
#include <hegel/cbor.h>
#include <protocol.h>

#include <stdexcept>
#include <string>
#include <unistd.h>

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
void Connection::handshake() {
    // Send "Hegel/1.0" as raw bytes on channel 0
    std::string version = "Hegel/1.0";
    std::vector<uint8_t> payload(version.begin(), version.end());
    uint32_t msg_id = alloc_message_id(0);
    protocol::write_packet(fd_, 0, msg_id, false, payload);

    // Wait for reply on channel 0
    auto pkt = wait_for(0, true);
    std::string response(pkt.payload.begin(), pkt.payload.end());
    if (response != "Ok") {
        throw std::runtime_error("hegel: version negotiation failed: " +
                                 response);
    }
}

// =============================================================================
// Request / Reply
// =============================================================================
cbor::Value Connection::request(uint32_t channel, const cbor::Value& msg) {
    auto payload = cbor::encode(msg);
    uint32_t msg_id = alloc_message_id(channel);
    protocol::write_packet(fd_, channel, msg_id, false, payload);

    auto pkt = wait_for(channel, true);
    return cbor::decode(pkt.payload);
}

void Connection::send_reply(uint32_t channel, uint32_t message_id,
                            const cbor::Value& msg) {
    auto payload = cbor::encode(msg);
    protocol::write_packet(fd_, channel, message_id, true, payload);
}

IncomingRequest Connection::recv_request(uint32_t channel) {
    auto pkt = wait_for(channel, false);

    // Check for close-channel signal
    if (pkt.payload.size() == 1 && pkt.payload[0] == protocol::CLOSE_PAYLOAD) {
        throw std::runtime_error("hegel: channel closed by server");
    }

    return IncomingRequest{pkt.message_id, cbor::decode(pkt.payload)};
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
            auto pkt = std::move(*it);
            queue.erase(it);
            return pkt;
        }
    }

    // Read packets until we get the one we want
    while (true) {
        auto pkt = protocol::read_packet(fd_);
        if (pkt.channel == channel && pkt.is_reply == want_reply) {
            return pkt;
        }
        pending_[pkt.channel].push_back(std::move(pkt));
    }
}

} // namespace hegel::impl
