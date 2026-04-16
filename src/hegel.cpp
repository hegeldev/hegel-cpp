/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <hegel/hegel.h>
#include <hegel/internal.h>
#include <hegel/json.h>
#include <hegel/settings.h>

#include "json_impl.h"

#include <connection.h>
#include <functional>
#include <protocol.h>
#include <stdexcept>
#include <test_case.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
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
                            const settings::HegelSettings& settings) {
        // Wire pipes to stdin/stdout for --stdio mode
        dup2(child_read_fd, STDIN_FILENO);
        dup2(child_write_fd, STDOUT_FILENO);
        ::close(child_read_fd);
        ::close(child_write_fd);

        std::string hegel_path = HEGEL_DEFAULT_PATH;

        std::vector<std::string> args = {
            hegel_path, "--stdio", "--verbosity",
            settings::verbosity_to_string(settings.verbosity)};

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        // execvp only returns on failure
        fprintf(stderr, "Failed to run Hegel server at path %s: %s\n",
                hegel_path.c_str(), strerror(errno));
        _exit(1);
    }

    static void hegel_parent(const std::function<void()>& test_fn,
                             pid_t child_pid, // NOLINT(misc-include-cleaner)
                             int read_fd, int write_fd,
                             const settings::HegelSettings& settings) {
        impl::Connection conn(read_fd, write_fd);

        conn.handshake();

        impl::protocol::init_protocol_debug(settings.verbosity);

        // Create test stream and start test
        uint32_t test_stream = conn.create_stream();
        uint64_t test_cases = settings.test_cases.value_or(100);

        hegel::internal::json::json run_test_msg = {{"command", "run_test"},
                                                    {"test_cases", test_cases},
                                                    {"stream_id", test_stream}};
        if (settings.seed.has_value()) {
            run_test_msg["seed"] = settings.seed.value();
        } else {
            run_test_msg["seed"] = nullptr;
        }
        run_test_msg["derandomize"] = settings.derandomize;
        switch (settings.database.kind()) {
        case hegel::settings::Database::Kind::Unset:
            break;
        case hegel::settings::Database::Kind::Disabled:
            run_test_msg["database"] = nullptr;
            break;
        case hegel::settings::Database::Kind::Path:
            run_test_msg["database"] = settings.database.path();
            break;
        }
        if (!settings.suppress_health_check.empty()) {
            auto arr = hegel::internal::json::json::array();
            for (auto c : settings.suppress_health_check) {
                arr.push_back(
                    std::string(hegel::settings::health_check_to_string(c)));
            }
            run_test_msg["suppress_health_check"] = arr;
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

                uint32_t data_stream = payload.value("stream_id", uint32_t{0});
                bool is_final = payload.value("is_final", false);

                // Set up per-test-case state
                impl::test_case::TestCaseData data{
                    .connection = &conn,
                    .data_stream = data_stream,
                    .is_last_run = is_final,
                    .test_aborted = false,
                    .verbosity = settings.verbosity,
                };
                impl::test_case::set(&data);

                // Run test
                std::string status = "VALID";
                std::string origin;
                try {
                    test_fn();
                } catch (const internal::HegelReject&) {
                    status = "INVALID";
                } catch (const std::exception& e) {
                    status = "INTERESTING";
                    origin = typeid(e).name();
                } catch (...) {
                    status = "INTERESTING";
                    if (const std::type_info* tinfo =
                            abi::__cxa_current_exception_type()) {
                        origin = tinfo->name();
                    } else {
                        origin = "unknown_exception";
                    }
                }

                // Clear per-test-case state
                impl::test_case::clear();

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

        // Cleanup: close pipes before waiting for child
        conn.close();

        int status;
        waitpid(child_pid, &status, 0);

        if (!test_passed) {
            throw std::runtime_error("Hegel test failed");
        }
    }

    void hegel(const std::function<void()>& test_fn,
               const settings::HegelSettings& settings) {
        // Create pipes for parent<->child stdio communication
        // parent_to_child: parent writes to [1], child reads from [0]
        // child_to_parent: child writes to [1], parent reads from [0]
        int parent_to_child[2];
        int child_to_parent[2];
        if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
            throw std::runtime_error("Failed to create pipes");
        }

        pid_t pid = fork();
        if (pid < 0) {
            throw std::runtime_error("Failed to fork");
        }

        if (pid == 0) {
            // Child: close unused pipe ends
            ::close(parent_to_child[1]);
            ::close(child_to_parent[0]);
            hegel_child(parent_to_child[0], child_to_parent[1], settings);
        } else {
            // Parent: close unused pipe ends
            ::close(parent_to_child[0]);
            ::close(child_to_parent[1]);
            hegel_parent(test_fn, pid, child_to_parent[0], parent_to_child[1],
                         settings);
        }
    }
} // namespace hegel
