#include <connection.h>
#include <data.h>
#include <data_source.h>
#include <hegel/internal.h>
#include <hegel/json.h>
#include <iostream>
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

    Connection::Connection(int read_fd, int write_fd)
        : read_fd_(read_fd), write_fd_(write_fd) {}

    Connection::~Connection() { close(); }

    void Connection::close() {
        if (read_fd_ >= 0) {
            ::close(read_fd_);
            read_fd_ = -1;
        }
        if (write_fd_ >= 0 && write_fd_ != read_fd_) {
            ::close(write_fd_);
            write_fd_ = -1;
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
    static constexpr const char* MIN_PROTOCOL_VERSION = "0.10";
    static constexpr const char* MAX_PROTOCOL_VERSION = "0.10";
    static constexpr const char* HANDSHAKE_STRING = "hegel_handshake_start";

    // Compare two "major.minor" version strings numerically.
    // Returns -1 if a < b, 0 if a == b, 1 if a > b.
    static int compare_versions(const std::string& a, const std::string& b) {
        auto parse = [](const std::string& s) -> std::pair<int, int> {
            auto dot = s.find('.');
            if (dot == std::string::npos ||
                s.find('.', dot + 1) != std::string::npos) {
                throw std::invalid_argument(
                    "invalid version string '" + s +     // GCOVR_EXCL_LINE
                    "': expected 'major.minor' format"); // GCOVR_EXCL_LINE
            }
            auto major_str = s.substr(0, dot);
            auto minor_str = s.substr(dot + 1);
            if (major_str.empty() || minor_str.empty()) {
                throw std::invalid_argument(
                    "invalid version string '" + s +     // GCOVR_EXCL_LINE
                    "': expected 'major.minor' format"); // GCOVR_EXCL_LINE
            }
            int major = std::stoi(major_str);
            int minor = std::stoi(minor_str);
            return {major, minor};
        };
        auto [a_major, a_minor] = parse(a);
        auto [b_major, b_minor] = parse(b);
        if (a_major != b_major)
            return a_major < b_major ? -1 : 1;
        if (a_minor != b_minor)
            return a_minor < b_minor ? -1 : 1;
        return 0;
    }

    void Connection::handshake() {
        std::string hs(HANDSHAKE_STRING);
        std::vector<uint8_t> payload(hs.begin(), hs.end());
        uint32_t msg_id = alloc_message_id(0);
        protocol::write_packet(write_fd_, 0, msg_id, false, payload);

        // Wait for reply on stream 0
        auto packet = wait_for(0, true);
        std::string response(packet.payload.begin(), packet.payload.end());

        std::string prefix = "Hegel/";
        if (!response.starts_with(prefix)) {
            throw std::runtime_error("Bad handshake response: " + response);
        }

        std::string server_version = response.substr(prefix.size());
        if (compare_versions(server_version, MIN_PROTOCOL_VERSION) < 0 ||
            compare_versions(server_version, MAX_PROTOCOL_VERSION) > 0) {
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
    hegel::internal::json::json
    Connection::request(uint32_t stream,
                        const hegel::internal::json::json& msg) {
        auto payload = protocol::cbor_encode(ImplUtil::raw(msg));
        uint32_t msg_id = alloc_message_id(stream);
        protocol::write_packet(write_fd_, stream, msg_id, false, payload);

        auto packet = wait_for(stream, true);
        auto result = protocol::cbor_decode(packet.payload);
        return ImplUtil::create(result);
    }

    void Connection::write_reply(uint32_t stream, uint32_t message_id,
                                 const hegel::internal::json::json& msg) {
        auto payload = protocol::cbor_encode(ImplUtil::raw(msg));
        protocol::write_packet(write_fd_, stream, message_id, true, payload);
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
        protocol::write_packet(write_fd_, stream, protocol::CLOSE_MESSAGE_ID,
                               false, payload);
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
            auto packet = protocol::read_packet(read_fd_);
            if (packet.stream == stream && packet.is_reply == want_reply) {
                return packet;
            }
            pending_[packet.stream].push_back(std::move(packet));
        }
    }

} // namespace hegel::impl

// =============================================================================
// Core Communication
// =============================================================================
namespace hegel::internal {

    static void handle_error_response(const nlohmann::json& response_raw,
                                      impl::data::TestCaseData* data) {
        if (!response_raw.contains("error")) {
            return;
        }
        std::string error_type;
        if (response_raw.contains("type") && response_raw["type"].is_string()) {
            error_type = response_raw["type"].get<std::string>();
        }
        if (error_type == "StopTest" || error_type == "Overflow") {
            data->test_aborted = true;
            internal::stop_test();
        }
        std::string error_msg = response_raw["error"].is_string()
                                    ? response_raw["error"].get<std::string>()
                                    : "unknown error";
        throw std::runtime_error(error_msg);
    }

    hegel::internal::json::json
    communicate_with_core(const hegel::internal::json::json& schema,
                          impl::data::TestCaseData* data) {
        if (impl::protocol::protocol_debug_enabled()) {
            hegel::internal::json::json dbg_req = {{"command", "generate"},
                                                   {"schema", schema}};
            std::cerr << "REQUEST: " << dbg_req.dump() << "\n";
        }

        hegel::internal::json::json response = data->source->generate(schema);

        auto response_raw = ImplUtil::raw(response);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "RESPONSE: " << response_raw.dump() << "\n";
        }

        handle_error_response(response_raw, data);

        // Auto-log generated value during final replay (counterexample)
        if (data->is_last_run) {
            if (response_raw.contains("result")) {
                std::cerr << "Generated: " << response_raw["result"].dump()
                          << "\n";
            }
        }

        return response;
    }

    void start_span(uint64_t label, impl::data::TestCaseData* data) {
        data->source->start_span(label);
    }

    void stop_span(bool discard, impl::data::TestCaseData* data) {
        data->source->stop_span(discard);
    }

} // namespace hegel::internal
