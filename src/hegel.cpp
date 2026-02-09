/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <base64.h>
#include <connection.h>
#include <functional>
#include <hegel/cbor.h>
#include <hegel/generators.h>
#include <hegel/internal.h>
#include <hegel/options.h>
#include <hegel/strategies.h>
#include <iostream>
#include <run_state.h>
#include <socket.h>
#include <stdexcept>
#include <thread>

// POSIX headers
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

namespace hegel {

using impl::cbor::as_bool;
using impl::cbor::as_text;
using impl::cbor::as_uint;
using impl::cbor::boolean;
using impl::cbor::integer;
using impl::cbor::map;
using impl::cbor::map_get;
using impl::cbor::null;
using impl::cbor::text;
using impl::cbor::Value;

// =============================================================================
// Child Process
// =============================================================================
static void hegel_child(const std::string& socket_path,
                        const options::HegelOptions& options) {
    std::string hegel_path = options.hegel_path.value_or(HEGEL_DEFAULT_PATH);
    uint64_t test_cases = options.test_cases.value_or(100);

    std::vector<std::string> args = {
        hegel_path,     socket_path,
        "--verbosity",  options::verbosity_to_string(options.verbosity),
        "--test-cases", std::to_string(test_cases)};

    std::vector<char*> argv;
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    int result = execvp(argv[0], argv.data());
    if (result == -1) {
        std::cerr << "Failed to run Hegel server at path " << hegel_path.c_str()
                  << ": " << strerror(errno) << std::endl;
    }
    std::exit(1);
}

// =============================================================================
// Wait for Socket File
// =============================================================================
static bool wait_for_socket(const std::string& path, int timeout_ms) {
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

// =============================================================================
// Connect to Unix Socket
// =============================================================================
static int connect_to_socket(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("hegel: failed to create socket");
    }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        close(fd);
        throw std::runtime_error("hegel: socket path too long");
    }
    std::copy(path.begin(), path.end(), addr.sun_path);
    addr.sun_path[path.size()] = '\0';

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <
        0) {
        close(fd);
        throw std::runtime_error("hegel: failed to connect to " + path);
    }

    return fd;
}

// =============================================================================
// Parent Process (SDK Client)
// =============================================================================
static void hegel_parent(const std::string& socket_path,
                         std::function<void()> test_fn, std::string temp_dir,
                         pid_t child_pid,
                         const options::HegelOptions& options) {
    // Wait for hegeld to create the socket
    if (!wait_for_socket(socket_path, 10000)) {
        // Check if child died
        int status;
        if (waitpid(child_pid, &status, WNOHANG) == child_pid) {
            std::filesystem::remove_all(temp_dir);
            throw std::runtime_error(
                "hegel: hegeld exited before creating socket (exit code " +
                std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1) +
                ")");
        }
        std::filesystem::remove_all(temp_dir);
        throw std::runtime_error("hegel: timed out waiting for socket at " +
                                 socket_path);
    }

    // Connect as client
    int fd = connect_to_socket(socket_path);
    impl::Connection conn(fd);

    // Version negotiation
    conn.handshake();

    // Create test channel and start test
    uint32_t test_channel = conn.create_channel();
    uint64_t test_cases = options.test_cases.value_or(100);

    Value run_test_msg = map({{"command", text("run_test")},
                              {"name", text("test")},
                              {"test_cases", integer(test_cases)},
                              {"channel", integer(test_channel)}});
    conn.request(0, run_test_msg);

    // Event loop on test channel
    bool test_passed = true;
    int final_replays_remaining = 0;
    bool done = false;
    while (!done) {
        auto event = conn.recv_request(test_channel);
        auto& payload = event.payload;

        auto event_type = as_text(map_get(payload, "event"));

        if (event_type == "test_case") {
            // Acknowledge test_case event
            conn.send_reply(test_channel, event.message_id,
                            map({{"result", null()}}));

            uint32_t data_channel = static_cast<uint32_t>(
                as_uint(map_get(payload, "channel")).value_or(0));
            bool is_final =
                as_bool(map_get(payload, "is_final")).value_or(false);

            // Set thread-local state
            impl::run_state::set_is_last_run(is_final);
            impl::socket::set_embedded_connection(&conn, data_channel);

            // Run test
            std::string status = "VALID";
            std::string origin;
            try {
                test_fn();
            } catch (const internal::HegelReject&) {
                status = "INVALID";
            } catch (const std::exception& e) {
                status = "INTERESTING";
                origin = e.what();
            } catch (...) {
                status = "INTERESTING";
                origin = "Unknown exception";
            }

            // Clear thread-local state
            impl::socket::clear_embedded_connection();

            // Send mark_complete and close data channel (unless aborted)
            if (!impl::socket::is_test_aborted()) {
                Value origin_value = origin.empty() ? null() : text(origin);
                Value mark = map({{"command", text("mark_complete")},
                                  {"status", text(status)},
                                  {"origin", origin_value}});
                conn.request(data_channel, mark);
                conn.close_channel(data_channel);
            }
            impl::socket::set_test_aborted(false);

            if (is_final) {
                final_replays_remaining--;
                if (final_replays_remaining <= 0) {
                    done = true;
                }
            }

        } else if (event_type == "test_done") {
            // Acknowledge test_done event
            conn.send_reply(test_channel, event.message_id,
                            map({{"result", boolean(true)}}));

            auto results = map_get(payload, "results");
            if (results) {
                test_passed =
                    as_bool(map_get(*results, "passed")).value_or(true);
                final_replays_remaining = static_cast<int>(
                    as_uint(map_get(*results, "interesting_test_cases"))
                        .value_or(0));
            }
            if (final_replays_remaining <= 0) {
                done = true;
            }
        }
    }

    // Cleanup: release socket before waiting for child
    conn.close();

    int status;
    waitpid(child_pid, &status, 0);
    std::filesystem::remove_all(temp_dir);

    if (!test_passed) {
        throw std::runtime_error("Hegel test failed");
    }
}

// =============================================================================
// Entry Point
// =============================================================================
void hegel(std::function<void()> test_fn, options::HegelOptions options) {
    // Create temp directory with socket path
    std::string temp_dir = "/tmp/hegel_" + std::to_string(getpid());
    std::filesystem::create_directories(temp_dir);
    std::string socket_path = temp_dir + "/hegel.sock";

    // Fork and exec hegeld
    pid_t pid = fork();
    if (pid < 0) {
        std::filesystem::remove_all(temp_dir);
        throw std::runtime_error("Failed to fork");
    }

    if (pid == 0) {
        hegel_child(socket_path, options);
    } else {
        hegel_parent(socket_path, std::move(test_fn), std::move(temp_dir), pid,
                     options);
    }
}
} // namespace hegel
