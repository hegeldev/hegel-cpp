#include <connection.h>
#include <hegel/internal.h>
#include <iostream>
#include <protocol.h>
#include <socket.h>
#include <stdexcept>
#include <data.h>
#include <thread>

#include <chrono>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
    // =============================================================================
    // Socket Setup
    // =============================================================================
    bool wait_for_socket(const std::string& path, int timeout_ms) {
        int elapsed = 0;
        while (elapsed < timeout_ms) {
            struct stat st;
            if (stat(path.c_str(), &st) == 0) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            elapsed += 100;
        }
        return false;
    }

    int connect_to_socket(const std::string& path) {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path.size() >= sizeof(addr.sun_path)) {
            ::close(fd);
            throw std::runtime_error("Socket path too long");
        }
        std::copy(path.begin(), path.end(), addr.sun_path);
        addr.sun_path[path.size()] = '\0';

        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                      sizeof(addr)) < 0) {
            ::close(fd);
            throw std::runtime_error("Failed to connect to " + path);
        }

        return fd;
    }
} // namespace hegel::impl::socket

namespace hegel::internal {
    nlohmann::json
    communicate_with_socket(const nlohmann::json& schema,
                            impl::data::TestCaseData* data) {
        auto* conn = data->connection;
        uint32_t data_channel = data->data_channel;

        // Build generate request as CBOR
        nlohmann::json request = {{"command", "generate"}, {"schema", schema}};

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "REQUEST: " << request.dump() << "\n";
        }

        // Send request and get response
        nlohmann::json response = conn->request(data_channel, request);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "RESPONSE: " << response.dump() << "\n";
        }

        // Handle errors
        if (response.contains("error")) {
            std::string error_type = response.value("type", "");
            if (error_type == "StopTest" || error_type == "Overflow") {
                data->test_aborted = true;
                internal::stop_test();
            }
            std::string error_msg = response["error"].is_string()
                                        ? response["error"].get<std::string>()
                                        : "unknown error";
            throw std::runtime_error(error_msg);
        }

        // Auto-log generated value during final replay (counterexample)
        if (data->is_last_run) {
            if (response.contains("result")) {
                std::cerr << "Generated: " << response["result"].dump() << "\n";
            }
        }

        return response;
    }

} // namespace hegel::internal
