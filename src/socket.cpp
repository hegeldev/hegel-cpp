#include <hegel/internal.h>
#include <hegel/options.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <run_state.h>
#include <socket.h>

// POSIX headers
#include <sys/socket.h>
#include <unistd.h>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
    // =============================================================================
    // Thread-local State
    // =============================================================================
    static thread_local int embedded_fd_ = -1;
    static thread_local std::string embedded_read_buffer_;

    // =============================================================================
    // Functions
    // =============================================================================
    void set_embedded_connection(int fd) {
        embedded_fd_ = fd;
        embedded_read_buffer_.clear();
    }

    void clear_embedded_connection() {
        embedded_fd_ = -1;
        embedded_read_buffer_.clear();
    }

    int get_embedded_fd() { return embedded_fd_; }

    std::string& get_embedded_read_buffer() { return embedded_read_buffer_; }

    std::string read_line(int fd) {
        std::string line;
        char c;
        while (read(fd, &c, 1) == 1) {
            if (c == '\n') {
                break;
            }
            line += c;
        }
        return line;
    }

    void write_line(int fd, const std::string& line) {
        std::string msg = line + "\n";
        size_t total = 0;
        while (total < msg.size()) {
            // Use send() with MSG_NOSIGNAL to prevent SIGPIPE on closed socket
            ssize_t n =
                send(fd, msg.data() + total, msg.size() - total, MSG_NOSIGNAL);
            if (n < 0) {
                break;
            }
            total += static_cast<size_t>(n);
        }
    }
} // namespace hegel::impl::socket

// TODO: we shouldn't be doing a lot of exits in a library; figure out why we
// are
// =============================================================================
// Constants
// =============================================================================

inline constexpr int ASSERTION_FAILURE_EXIT_CODE = 134;

namespace hegel::internal {
    std::string communicate_with_socket(const std::string& schema) {
        nlohmann::json payload = nlohmann::json::parse(schema);

        int fd = impl::socket::get_embedded_fd();
        if (fd < 0) {
            std::cerr << "hegel: no connection set\n";
            std::exit(ASSERTION_FAILURE_EXIT_CODE);
        }

        // Build and send request
        static thread_local int request_counter = 0;
        int request_id = ++request_counter;
        nlohmann::json request = {
            {"id", request_id}, {"command", "generate"}, {"payload", payload}};
        std::string message = request.dump() + "\n";

        if (std::getenv("HEGEL_DEBUG")) {
            std::cerr << "REQUEST: " << message;
        }

        // Send
        size_t total_sent = 0;
        while (total_sent < message.size()) {
            ssize_t sent = write(fd, message.data() + total_sent,
                                 message.size() - total_sent);
            if (sent < 0) {
                std::cerr << "hegel: failed to write to socket\n";
                std::exit(ASSERTION_FAILURE_EXIT_CODE);
            }
            total_sent += static_cast<size_t>(sent);
        }

        // Read response
        std::string& buffer = impl::socket::get_embedded_read_buffer();
        while (true) {
            size_t newline = buffer.find('\n');
            if (newline != std::string::npos) {
                std::string line = buffer.substr(0, newline);
                buffer.erase(0, newline + 1);

                if (std::getenv("HEGEL_DEBUG")) {
                    std::cerr << "RESPONSE: " << line << "\n";
                }

                auto response = nlohmann::json::parse(line);
                if (response.contains("reject")) {
                    // Rejection: hypothesis stopped this test case (e.g.,
                    // buffer exhausted). Just stop the test - hegel already
                    // knows it's a rejection.
                    internal::stop_test();
                }
                if (response.contains("error")) {
                    // Genuine error: bad schema, invalid request, etc.
                    // Throw runtime_error to propagate as a real test failure.
                    throw std::runtime_error(
                        "hegel: " + response["error"].get<std::string>());
                }
                // Auto-log generated value during final replay (counterexample)
                if (impl::run_state::is_last_run()) {
                    std::cerr << "Generated: " << response["result"].dump()
                              << "\n";
                }
                // Return full response for parsing by Generator::generate_impl
                return response.dump();
            }

            char chunk[4096];
            ssize_t n = read(fd, chunk, sizeof(chunk));
            if (n <= 0) {
                std::cerr
                    << "hegel: connection closed while reading response (n="
                    << n << ", errno=" << errno << ", fd=" << fd << ")\n";
                std::cerr << "hegel: buffer so far: " << buffer << "\n";
                std::exit(ASSERTION_FAILURE_EXIT_CODE);
            }
            buffer.append(chunk, static_cast<size_t>(n));
        }
    }

} // namespace hegel::internal
