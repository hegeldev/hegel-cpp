#include <connection.h>
#include <data.h>
#include <hegel/internal.h>
#include <iostream>
#include <protocol.h>
#include <socket.h>
#include <stdexcept>
#include <thread>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <hegel/json.h>
#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "json_impl.h"

using hegel::internal::json::ImplUtil;

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
    // =============================================================================
    // Socket Setup
    // =============================================================================
    int connect_to_socket(const std::string& path, int timeout_ms) {
#ifdef SOCK_CLOEXEC
        int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
#endif
        if (fd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
#ifndef SOCK_CLOEXEC
        fcntl(fd, F_SETFD, FD_CLOEXEC); // NOLINT(misc-include-cleaner)
#endif

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path.size() >= sizeof(addr.sun_path)) {
            ::close(fd);
            throw std::runtime_error("Socket path too long");
        }
        std::copy(path.begin(), path.end(), addr.sun_path);
        addr.sun_path[path.size()] = '\0';

        int elapsed = 0;
        while (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(addr)) < 0) {
            if (elapsed >= timeout_ms) {
                int err = errno;
                ::close(fd);
                throw std::runtime_error("Failed to connect to " + path + ": " +
                                         strerror(err));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            elapsed += 100;
        }

        return fd;
    }
} // namespace hegel::impl::socket

namespace hegel::internal {
    hegel::internal::json::json
    communicate_with_socket(const hegel::internal::json::json& schema,
                            impl::data::TestCaseData* data) {
        auto* conn = data->connection;
        uint32_t data_stream = data->data_stream;

        // Build generate request as CBOR
        hegel::internal::json::json request = {{"command", "generate"},
                                               {"schema", schema}};

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "REQUEST: " << request.dump() << "\n";
        }

        // Send request and get response
        hegel::internal::json::json response =
            conn->request(data_stream, request);

        auto response_raw = ImplUtil::raw(response);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "RESPONSE: " << response_raw.dump() << "\n";
        }

        // Handle errors
        if (response_raw.contains("error")) {
            std::string error_type = response_raw.value("type", "");
            if (error_type == "StopTest" || error_type == "Overflow") {
                data->test_aborted = true;
                internal::stop_test();
            }
            std::string error_msg =
                response_raw["error"].is_string()
                    ? response_raw["error"].get<std::string>()
                    : "unknown error";
            throw std::runtime_error(error_msg);
        }

        // Auto-log generated value during final replay (counterexample)
        if (data->is_last_run) {
            if (response_raw.contains("result")) {
                std::cerr << "Generated: " << response_raw["result"].dump()
                          << "\n";
            }
        }

        return response;
    }

} // namespace hegel::internal
