/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <hegel/hegel.h>

#include <base64.h>
#include <connection.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <run_state.h>
#include <socket.h>
#include <stdexcept>
#include <thread>

#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

namespace hegel {

    // =============================================================================
    // Child Process
    // =============================================================================
    static void hegel_child(const std::string& socket_path,
                            const options::HegelOptions& options) {
        std::string hegel_path =
            options.hegel_path.value_or(HEGEL_DEFAULT_PATH);
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
            std::cerr << "Failed to run Hegel server at path "
                      << hegel_path.c_str() << ": " << strerror(errno)
                      << std::endl;
        }
        std::exit(1);
    }

    // =============================================================================
    // Parent Process (SDK Client)
    // =============================================================================
    static void hegel_parent(const std::string& socket_path,
                             std::function<void()> test_fn,
                             std::string temp_dir, pid_t child_pid,
                             const options::HegelOptions& options) {
        // Wait for hegeld to create the socket
        if (!impl::socket::wait_for_socket(socket_path, 10000)) {
            // Check if child died
            int status;
            if (waitpid(child_pid, &status, WNOHANG) == child_pid) {
                std::filesystem::remove_all(temp_dir);
                throw std::runtime_error(
                    "hegel: hegeld exited before creating socket (exit code " +
                    std::to_string(WIFEXITED(status) ? WEXITSTATUS(status)
                                                     : -1) +
                    ")");
            }
            std::filesystem::remove_all(temp_dir);
            throw std::runtime_error("hegel: timed out waiting for socket at " +
                                     socket_path);
        }

        // Connect as client
        int fd = impl::socket::connect_to_socket(socket_path);
        impl::Connection conn(fd);

        // Version negotiation
        conn.handshake();

        // Create test channel and start test
        uint32_t test_channel = conn.create_channel();
        uint64_t test_cases = options.test_cases.value_or(100);

        nlohmann::json run_test_msg = {{"command", "run_test"},
                                       {"name", "test"},
                                       {"test_cases", test_cases},
                                       {"channel", test_channel}};
        conn.request(0, run_test_msg);

        // Event loop on test channel
        bool test_passed = true;
        int final_replays_remaining = 0;
        bool done = false;
        while (!done) {
            auto event = conn.recv_request(test_channel);
            auto& payload = event.payload;

            std::string event_type = payload.value("event", "");

            if (event_type == "test_case") {
                // Acknowledge test_case event
                conn.send_reply(test_channel, event.message_id,
                                nlohmann::json{{"result", nullptr}});

                uint32_t data_channel = payload.value("channel", uint32_t{0});
                bool is_final = payload.value("is_final", false);

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
                    nlohmann::json origin_value = origin.empty()
                                                      ? nlohmann::json(nullptr)
                                                      : nlohmann::json(origin);
                    nlohmann::json mark = {{"command", "mark_complete"},
                                           {"status", status},
                                           {"origin", origin_value}};
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
                                nlohmann::json{{"result", true}});

                if (payload.contains("results")) {
                    auto& results = payload["results"];
                    test_passed = results.value("passed", true);
                    final_replays_remaining =
                        results.value("interesting_test_cases", 0);
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
            hegel_parent(socket_path, std::move(test_fn), std::move(temp_dir),
                         pid, options);
        }
    }
} // namespace hegel
