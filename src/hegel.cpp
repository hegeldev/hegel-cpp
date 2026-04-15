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
#include <data_source.h>
#include <functional>
#include <protocol.h>
#include <stdexcept>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

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
    static void hegel_child(int child_read_fd, int child_write_fd,
                            const options::HegelOptions& options) {
        // Wire pipes to stdin/stdout for --stdio mode
        dup2(child_read_fd, STDIN_FILENO);
        dup2(child_write_fd, STDOUT_FILENO);
        ::close(child_read_fd);
        ::close(child_write_fd);

        std::string hegel_path =
            options.hegel_path.value_or(HEGEL_DEFAULT_PATH);

        std::vector<std::string> args = {
            hegel_path, "--stdio", "--verbosity",
            options::verbosity_to_string(options.verbosity)};

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        // execvp only returns on failure; child calls _exit so gcov never
        // flushes
        fprintf(
            stderr,
            "Failed to run Hegel server at path %s: %s\n", // GCOVR_EXCL_LINE
            hegel_path.c_str(), strerror(errno));          // GCOVR_EXCL_LINE
        _exit(1);                                          // GCOVR_EXCL_LINE
    }

    // =============================================================================
    // Parent Process (Client)
    // =============================================================================
    static void hegel_parent(const std::function<void()>& test_fn,
                             pid_t child_pid, // NOLINT(misc-include-cleaner)
                             int read_fd, int write_fd,
                             const options::HegelOptions& options) {
        impl::Connection conn(read_fd, write_fd);

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
        while (!done) {
            auto event = conn.recv_request(test_stream);
            auto& payload = event.payload;

            std::string event_type = payload.value("event", "");

            if (event_type == "test_case") {
                // Acknowledge test_case event
                conn.write_reply(
                    test_stream, event.message_id,
                    hegel::internal::json::json{{"result", nullptr}});

                auto& payload_raw = ImplUtil::raw(payload);
                uint32_t data_stream = 0;
                if (payload_raw.contains("stream_id") &&
                    payload_raw["stream_id"].is_number()) {
                    data_stream = payload_raw["stream_id"].get<uint32_t>();
                }
                bool is_final = false;
                if (payload_raw.contains("is_final") &&
                    payload_raw["is_final"].is_boolean()) {
                    is_final = payload_raw["is_final"].get<bool>();
                }

                // Set up per-test-case state
                impl::ConnectionDataSource source(&conn, data_stream);
                impl::data::TestCaseData data{
                    .source = &source,
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
                    try {
                        auto mark_response = conn.request(data_stream, mark);
                        auto mark_raw = ImplUtil::raw(mark_response);
                        if (mark_raw.contains("error")) {
                            std::string error_type = mark_raw.value("type", "");
                            if (error_type == "StopTest" || // GCOVR_EXCL_START
                                error_type == "Overflow") {
                                data.test_aborted = true;
                            } // GCOVR_EXCL_STOP
                        }
                    } catch (const internal::HegelReject&) { // GCOVR_EXCL_START
                        data.test_aborted = true;
                    } catch (const std::exception&) {
                        // mark_complete failed; treat as aborted
                        data.test_aborted = true;
                    }
                    // GCOVR_EXCL_STOP
                    if (!data.test_aborted) {
                        conn.close_stream(data_stream); // GCOVR_EXCL_LINE
                    }
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
                    if (results.is_object()) {
                        if (results.contains("passed") &&
                            results["passed"].is_boolean()) {
                            test_passed = results["passed"].get<bool>();
                        }
                        if (results.contains("interesting_test_cases") &&
                            results["interesting_test_cases"]
                                .is_number_integer()) {
                            final_replays_remaining =
                                results["interesting_test_cases"].get<int>();
                        }
                    }
                }

                if (final_replays_remaining <= 0) {
                    done = true;
                }
            }
        }

        // Cleanup: close pipes before waiting for child
        conn.close();

        int status;
        waitpid(child_pid, &status, 0);

        if (!test_passed) {
            throw std::runtime_error("Hegel test failed");
        }
    }

    // =============================================================================
    // Entry Point
    // =============================================================================
    void hegel(const std::function<void()>& test_fn,
               const options::HegelOptions& options) {
        // Create pipes for parent<->child stdio communication
        // parent_to_child: parent writes to [1], child reads from [0]
        // child_to_parent: child writes to [1], parent reads from [0]
        int parent_to_child[2];
        int child_to_parent[2];
        if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
            throw std::runtime_error(
                "Failed to create pipes"); // GCOVR_EXCL_LINE
        }

        pid_t pid = fork();
        if (pid < 0) {
            throw std::runtime_error("Failed to fork"); // GCOVR_EXCL_LINE
        }

        if (pid == 0) {
            // Child: close unused pipe ends
            ::close(parent_to_child[1]);
            ::close(child_to_parent[0]);
            hegel_child(parent_to_child[0], child_to_parent[1], options);
        } else {
            // Parent: close unused pipe ends
            ::close(parent_to_child[0]);
            ::close(child_to_parent[1]);
            hegel_parent(test_fn, pid, child_to_parent[0], parent_to_child[1],
                         options);
        }
    }
} // namespace hegel
