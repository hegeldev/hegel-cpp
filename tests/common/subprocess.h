#pragma once

/**
 * @file subprocess.h
 * @brief Helpers for running a prebuilt executable and capturing its stdio.
 *
 * Used by test_output.cpp to run subject binaries that deliberately fail,
 * and assert on the stderr produced by the hegel runtime.
 */

#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace hegel::tests::common {

    struct SubprocessResult {
        int exit_code = 0;
        std::string stdout_data;
        std::string stderr_data;
    };

    inline std::string drain_fd(int fd) {
        std::string out;
        char buf[4096];
        while (true) {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) {
                out.append(buf, buf + n);
            } else if (n == 0) {
                break;
            } else {
                if (errno == EINTR)
                    continue;
                break;
            }
        }
        return out;
    }

    /// Fork+exec a binary and drain its stdout/stderr pipes concurrently.
    /// `env_overrides` adds or replaces environment variables in the child;
    /// `env_removes` unsets variables in the child (applied after overrides).
    inline SubprocessResult run_subject(
        const std::string& path, const std::vector<std::string>& args = {},
        const std::vector<std::pair<std::string, std::string>>& env_overrides =
            {},
        const std::vector<std::string>& env_removes = {}) {
        int out_pipe[2];
        int err_pipe[2];
        if (::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0) {
            throw std::runtime_error("pipe() failed");
        }

        pid_t pid = ::fork();
        if (pid < 0) {
            throw std::runtime_error("fork() failed");
        }
        if (pid == 0) {
            ::close(out_pipe[0]);
            ::close(err_pipe[0]);
            ::dup2(out_pipe[1], STDOUT_FILENO);
            ::dup2(err_pipe[1], STDERR_FILENO);
            ::close(out_pipe[1]);
            ::close(err_pipe[1]);

            for (const auto& [k, v] : env_overrides) {
                ::setenv(k.c_str(), v.c_str(), 1);
            }
            for (const auto& k : env_removes) {
                ::unsetenv(k.c_str());
            }

            std::vector<std::string> argv_storage;
            argv_storage.push_back(path);
            for (const auto& a : args)
                argv_storage.push_back(a);

            std::vector<char*> argv;
            argv.reserve(argv_storage.size() + 1);
            for (auto& a : argv_storage)
                argv.push_back(a.data());
            argv.push_back(nullptr);

            ::execvp(argv[0], argv.data());
            // exec failed; emit diagnostic to stderr and exit.
            std::string msg = "execvp failed: ";
            msg += ::strerror(errno);
            msg += "\n";
            (void)!::write(STDERR_FILENO, msg.data(), msg.size());
            ::_exit(127);
        }

        ::close(out_pipe[1]);
        ::close(err_pipe[1]);

        SubprocessResult r;
        std::thread out_t([&] { r.stdout_data = drain_fd(out_pipe[0]); });
        std::thread err_t([&] { r.stderr_data = drain_fd(err_pipe[0]); });
        out_t.join();
        err_t.join();
        ::close(out_pipe[0]);
        ::close(err_pipe[0]);

        int status = 0;
        ::waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            r.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            r.exit_code = 128 + WTERMSIG(status);
        }
        return r;
    }

} // namespace hegel::tests::common
