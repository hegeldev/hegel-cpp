#pragma once

#include <hegel/json.h>
#include <protocol.h>

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

// =============================================================================
// Connection and Channel Multiplexing
// =============================================================================
namespace hegel::impl {

    struct IncomingRequest {
        uint32_t message_id;
        hegel::internal::json::json payload;
    };

    class Connection {
      public:
        explicit Connection(int fd);
        ~Connection();

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        /// Version handshake on channel 0
        void handshake();

        /// Allocate the next odd-numbered client channel
        uint32_t create_channel();

        /// Send a CBOR request and wait for the reply
        hegel::internal::json::json request(uint32_t channel, const hegel::internal::json::json& msg);

        /// Send a CBOR reply to a server-initiated request
        void write_reply(uint32_t channel, uint32_t message_id,
                         const hegel::internal::json::json& msg);

        /// Wait for a server-initiated request on a channel
        IncomingRequest recv_request(uint32_t channel);

        /// Send close-channel packet
        void close_channel(uint32_t channel);

        /// Close the socket
        void close();

      private:
        int fd_;
        uint32_t next_channel_counter_ = 0;
        std::unordered_map<uint32_t, uint32_t> next_message_id_;
        std::unordered_map<uint32_t, std::deque<protocol::Packet>> pending_;

        uint32_t alloc_message_id(uint32_t channel);
        protocol::Packet wait_for(uint32_t channel, bool want_reply);
    };

} // namespace hegel::impl
