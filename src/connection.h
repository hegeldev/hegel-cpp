#pragma once

#include <hegel/json.h>
#include <protocol.h>

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace hegel::impl {

    struct IncomingRequest {
        uint32_t message_id;
        hegel::internal::json::json payload;
    };

    class Connection {
      public:
        Connection(int read_fd, int write_fd);
        ~Connection();

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        /// Version handshake on stream 0
        void handshake();

        /// Allocate the next odd-numbered client stream
        uint32_t create_stream();

        /// Send a CBOR request and wait for the reply
        hegel::internal::json::json
        request(uint32_t stream, const hegel::internal::json::json& msg);

        /// Send a CBOR reply to a server-initiated request
        void write_reply(uint32_t stream, uint32_t message_id,
                         const hegel::internal::json::json& msg);

        /// Wait for a server-initiated request on a stream
        IncomingRequest recv_request(uint32_t stream);

        /// Send close-stream packet
        void close_stream(uint32_t stream);

        /// Close the socket
        void close();

      private:
        int read_fd_;
        int write_fd_;
        uint32_t next_stream_counter_ = 0;
        std::unordered_map<uint32_t, uint32_t> next_message_id_;
        std::unordered_map<uint32_t, std::deque<protocol::Packet>> pending_;

        uint32_t alloc_message_id(uint32_t stream);
        protocol::Packet wait_for(uint32_t stream, bool want_reply);
    };

} // namespace hegel::impl
