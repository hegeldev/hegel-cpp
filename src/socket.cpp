#include <connection.h>
#include <hegel/internal.h>
#include <hegel/options.h>
#include <iostream>
#include <run_state.h>
#include <socket.h>
#include <stdexcept>
#include <thread>

#include <algorithm>
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
    // Thread-local State
    // =============================================================================
    static thread_local Connection* embedded_conn_ = nullptr;
    static thread_local uint32_t embedded_data_channel_ = 0;
    static thread_local bool test_aborted_ = false;

    // =============================================================================
    // Functions
    // =============================================================================
    void set_embedded_connection(Connection* conn, uint32_t data_channel) {
        embedded_conn_ = conn;
        embedded_data_channel_ = data_channel;
        test_aborted_ = false;
    }

    void clear_embedded_connection() {
        embedded_conn_ = nullptr;
        embedded_data_channel_ = 0;
    }

    Connection* get_embedded_connection() { return embedded_conn_; }

    uint32_t get_embedded_data_channel() { return embedded_data_channel_; }

    bool is_test_aborted() { return test_aborted_; }

    void set_test_aborted(bool v) { test_aborted_ = v; }

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
            throw std::runtime_error("hegel: failed to create socket");
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path.size() >= sizeof(addr.sun_path)) {
            ::close(fd);
            throw std::runtime_error("hegel: socket path too long");
        }
        std::copy(path.begin(), path.end(), addr.sun_path);
        addr.sun_path[path.size()] = '\0';

        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                      sizeof(addr)) < 0) {
            ::close(fd);
            throw std::runtime_error("hegel: failed to connect to " + path);
        }

        return fd;
    }
} // namespace hegel::impl::socket

// =============================================================================
// Constants
// =============================================================================
inline constexpr int ASSERTION_FAILURE_EXIT_CODE = 134;

namespace hegel::internal {

    /// Send a command request and handle errors/StopTest.
    /// Returns the response JSON.
    static nlohmann::json send_command(const nlohmann::json& request) {
        auto* conn = impl::socket::get_embedded_connection();
        uint32_t data_channel = impl::socket::get_embedded_data_channel();

        if (!conn) {
            std::cerr << "hegel: no connection set\n";
            std::exit(ASSERTION_FAILURE_EXIT_CODE);
        }

        if (std::getenv("HEGEL_DEBUG")) {
            std::cerr << "REQUEST: " << request.dump() << "\n";
        }

        nlohmann::json response = conn->request(data_channel, request);

        if (std::getenv("HEGEL_DEBUG")) {
            std::cerr << "RESPONSE: " << response.dump() << "\n";
        }

        // Handle errors
        if (response.contains("error")) {
            std::string error_type = response.value("type", "");
            if (error_type == "StopTest" || error_type == "Overflow") {
                impl::socket::set_test_aborted(true);
                internal::stop_test();
            }
            std::string error_msg = response["error"].is_string()
                                        ? response["error"].get<std::string>()
                                        : "unknown error";
            throw std::runtime_error("hegel: " + error_msg);
        }

        return response;
    }

    nlohmann::json communicate_with_socket(const nlohmann::json& schema) {
        nlohmann::json request = {{"command", "generate"}, {"schema", schema}};

        nlohmann::json response = send_command(request);

        // Auto-log generated value during final replay (counterexample)
        if (impl::run_state::is_last_run()) {
            if (response.contains("result")) {
                std::cerr << "Generated: " << response["result"].dump() << "\n";
            }
        }

        return response;
    }

    void start_span(uint64_t label) {
        if (impl::socket::is_test_aborted())
            return;
        send_command({{"command", "start_span"}, {"label", label}});
    }

    void stop_span(bool discard) {
        if (impl::socket::is_test_aborted())
            return;
        // Ignore errors from stop_span - we're already closing
        try {
            send_command({{"command", "stop_span"}, {"discard", discard}});
        } catch (...) {
        }
    }

    void target(double value, const std::string& label) {
        send_command(
            {{"command", "target"}, {"value", value}, {"label", label}});
    }

} // namespace hegel::internal
