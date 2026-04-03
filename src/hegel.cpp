/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <hegel/hegel.h>
#include <hegel/internal.h>
#include <hegel/json.h>
#include <hegel/options.h>

#include "json_impl.h"

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
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

using hegel::internal::json::ImplUtil;

namespace hegel {

    // =============================================================================
    // Child Process
    // =============================================================================
    static void hegel_child(const std::string& socket_path,
                            const options::HegelOptions& options) {
        // Detach stdout/stderr so the hegel server doesn't hold
        // ctest's pipes open if the parent exits early.
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            ::close(devnull);
        }

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
        _exit(1);
    }

    // =============================================================================
    // Parent Process (Client)
    // =============================================================================
    static void hegel_parent(const std::string& socket_path,
                             const std::function<void()>& test_fn,
                             const std::string& temp_dir,
                             pid_t child_pid, // NOLINT(misc-include-cleaner)
                             const options::HegelOptions& options) {
        // Connect as client (retries until the server is ready)
        int fd;
        try {
            fd = impl::socket::connect_to_socket(socket_path, 10000);
        } catch (const std::runtime_error&) {
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
            throw;
        }
        impl::Connection conn(fd);

        // Version negotiation
        conn.handshake();

        impl::protocol::init_protocol_debug(options.verbosity);

        // Create test stream and start test
        uint32_t test_stream = conn.create_stream();
        uint64_t test_cases = options.test_cases.value_or(100);

        hegel::internal::json::json run_test_msg = {{"command", "run_test"},
                                                    {"test_cases", test_cases},
                                                    {"stream_id", test_stream}};
        if (options.seed.has_value()) {
            run_test_msg["seed"] = options.seed.value();
        } else {
            run_test_msg["seed"] = nullptr;
        }
        conn.request(0, run_test_msg);

        // Event loop on test stream
        bool test_passed = true;
        int final_replays_remaining = 0;
        bool done = false;
        int test_case_num = 0;
        while (!done) {
            auto event = conn.recv_request(test_stream);
            auto& payload = event.payload;

            std::string event_type = payload.value("event", "");

            if (event_type == "test_case") {
                test_case_num++;
                // Acknowledge test_case event
                conn.write_reply(
                    test_stream, event.message_id,
                    hegel::internal::json::json{{"result", nullptr}});

                uint32_t data_stream = payload.value("stream_id", uint32_t{0});
                bool is_final = payload.value("is_final", false);

                // Set up per-test-case state
                impl::data::TestCaseData data{
                    .connection = &conn,
                    .data_stream = data_stream,
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

                // Send mark_complete and close data stream (unless aborted)
                if (!data.test_aborted) {
                    hegel::internal::json::json origin_value =
                        origin.empty() ? hegel::internal::json::json(nullptr)
                                       : hegel::internal::json::json(origin);
                    hegel::internal::json::json mark = {
                        {"command", "mark_complete"},
                        {"status", status},
                        {"origin", origin_value}};
                    conn.request(data_stream, mark);
                    conn.close_stream(data_stream);
                }

                if (is_final) {
                    final_replays_remaining--;
                    if (final_replays_remaining <= 0) {
                        done = true;
                    }
                }

            } else if (event_type == "test_done") {
                // Acknowledge test_done event
                conn.write_reply(test_stream, event.message_id,
                                 hegel::internal::json::json{{"result", true}});

                if (payload.contains("results")) {
                    auto& results = ImplUtil::raw(payload["results"]);
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
