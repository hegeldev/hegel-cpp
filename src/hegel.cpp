/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <hegel/hegel.h>
#include <hegel/internal.h>
#include <hegel/options.h>

#include <connection.h>
#include <data.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <protocol.h>
#include <socket.h>
#include <stdexcept>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <vector>

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
            hegel_path, socket_path, "--verbosity",
            options::verbosity_to_string(options.verbosity)};

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
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
                             const std::function<void()>& test_fn,
                             const std::string& temp_dir,
                             pid_t child_pid, // NOLINT(misc-include-cleaner)
                             const options::HegelOptions& options) {
        // Wait for hegel-core to create the socket
        if (!impl::socket::wait_for_socket(socket_path, 10000)) {
            // Check if child died
            int status;
            // NOLINTNEXTLINE(misc-include-cleaner)
            if (waitpid(child_pid, &status, WNOHANG) == child_pid) {
                std::filesystem::remove_all(temp_dir);
                throw std::runtime_error(
                    "Hegel server exited before creating socket (exit code " +
                    // NOLINTNEXTLINE(misc-include-cleaner)
                    std::to_string(WIFEXITED(status) ? WEXITSTATUS(status)
                                                     : -1) +
                    ")");
            }
            std::filesystem::remove_all(temp_dir);
            throw std::runtime_error("Timed out waiting for socket at " +
                                     socket_path);
        }

        // Connect as client
        int fd = impl::socket::connect_to_socket(socket_path);
        impl::Connection conn(fd);

        // Version negotiation
        conn.handshake();

        impl::protocol::init_protocol_debug(options.verbosity);

        // Create test channel and start test
        uint32_t test_channel = conn.create_channel();
        uint64_t test_cases = options.test_cases.value_or(100);

        nlohmann::json run_test_msg = {{"command", "run_test"},
                                       {"test_cases", test_cases},
                                       {"channel_id", test_channel}};
        if (options.seed.has_value()) {
            run_test_msg["seed"] = options.seed.value();
        } else {
            run_test_msg["seed"] = nullptr;
        }

        if (options.failure_blob.has_value()) {
            run_test_msg["failure_blob"] = options.failure_blob.value();
        } else {
            run_test_msg["failure_blob"] = nullptr;
        }

        conn.request(0, run_test_msg);

        // Event loop on test channel
        bool test_passed = true;
        std::vector<std::string> failure_blobs;
        int final_replays_remaining = 0;
        bool done = false;
        while (!done) {
            auto event = conn.recv_request(test_channel);
            auto& payload = event.payload;

            std::string event_type = payload.value("event", "");

            if (event_type == "test_case") {
                // Acknowledge test_case event
                conn.write_reply(test_channel, event.message_id,
                                 nlohmann::json{{"result", nullptr}});

                uint32_t data_channel =
                    payload.value("channel_id", uint32_t{0});
                bool is_final = payload.value("is_final", false);

                // Set up per-test-case state
                impl::data::TestCaseData data{
                    .connection = &conn,
                    .data_channel = data_channel,
                    .is_last_run = is_final,
                    .test_aborted = false,
                    .verbosity = options.verbosity,
                };
                impl::data::set(&data);

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

                // Clear per-test-case state
                impl::data::clear();

                // Send mark_complete and close data channel (unless aborted)
                if (!data.test_aborted) {
                    nlohmann::json origin_value = origin.empty()
                                                      ? nlohmann::json(nullptr)
                                                      : nlohmann::json(origin);
                    nlohmann::json mark = {{"command", "mark_complete"},
                                           {"status", status},
                                           {"origin", origin_value}};
                    conn.request(data_channel, mark);
                    conn.close_channel(data_channel);
                }

                if (is_final) {
                    final_replays_remaining--;
                    if (final_replays_remaining <= 0) {
                        done = true;
                    }
                }

            } else if (event_type == "test_done") {
                // Acknowledge test_done event
                conn.write_reply(test_channel, event.message_id,
                                 nlohmann::json{{"result", true}});

                if (payload.contains("results")) {
                    auto& results = payload["results"];
                    test_passed = results.value("passed", true);
                    final_replays_remaining =
                        results.value("interesting_test_cases", 0);
                    for (const auto& blob : results["failure_blobs"]) {
                        auto byte_sequence = blob.get_binary();
                        std::string failure_blob_string(
                            reinterpret_cast<const char*>(byte_sequence.data()),
                            byte_sequence.size());
                        failure_blobs.push_back(failure_blob_string);
                    }
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

        if (test_passed && options.failure_blob.has_value()) {
            throw std::runtime_error("Failure blob did not cause a failure");
        }
        if (options.print_blob && !failure_blobs.empty()) {
            std::cerr << "Failure blobs for reproduction:\n";
            for (const auto& blob : failure_blobs) {
                std::cerr << blob << "\n";
            }
        }
        if (!test_passed) {
            throw std::runtime_error("Hegel test failed");
        }
    }

    // =============================================================================
    // Entry Point
    // =============================================================================
    void hegel(const std::function<void()>& test_fn,
               const options::HegelOptions& options) {
        // Create temp directory with socket path
        std::string temp_dir = "/tmp/hegel_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir);
        std::string socket_path = temp_dir + "/hegel.sock";

        pid_t pid = fork();
        if (pid < 0) {
            std::filesystem::remove_all(temp_dir);
            throw std::runtime_error("Failed to fork");
        }

        if (pid == 0) {
            hegel_child(socket_path, options);
        } else {
            hegel_parent(socket_path, test_fn, temp_dir, pid, options);
        }
    }
} // namespace hegel
