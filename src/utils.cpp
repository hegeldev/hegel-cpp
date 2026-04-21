#include "utils.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <spawn.h>
#include <stdexcept>
#include <stdlib.h> // IWYU pragma: keep
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h> // IWYU pragma: keep
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <crt_externs.h>
#endif

namespace fs = std::filesystem;

namespace {
    // Portable access to the process environment. macOS does not export
    // `environ` to library code; `_NSGetEnviron()` is the documented
    // alternative.
    inline char** process_environ() {
#if defined(__APPLE__)
        return *::_NSGetEnviron();
#else
        return ::environ;
#endif
    }

    std::string_view env_key(std::string_view entry) {
        auto eq = entry.find('=');
        return eq == std::string_view::npos ? entry : entry.substr(0, eq);
    }
} // namespace

namespace hegel::impl::utils {

    std::optional<std::string> which(const std::string& name) {
        const char* path_var = std::getenv("PATH");
        if (!path_var) {
            return std::nullopt;
        }
        std::string path(path_var);
        std::size_t start = 0;
        while (start <= path.size()) {
            std::size_t end = path.find(':', start);
            if (end == std::string::npos) {
                end = path.size();
            }
            if (end > start) {
                fs::path candidate =
                    fs::path(path.substr(start, end - start)) / name;
                std::error_code ec;
                if (fs::is_regular_file(candidate, ec)) {
                    return candidate.string();
                }
            }
            start = end + 1;
        }
        return std::nullopt;
    }

    void validate_executable(const std::string& path) {
        struct stat st{};
        if (::stat(path.c_str(), &st) == 0) {
            if ((st.st_mode & 0111) == 0) {
                throw std::runtime_error(
                    "Hegel server binary at '" + path +
                    "' is not executable. Check file permissions.");
            }
        }
    }

    TempFile::TempFile(std::string_view prefix) {
        std::string tmpl =
            (fs::temp_directory_path() / (std::string(prefix) + "-XXXXXX"))
                .string();
        std::vector<char> path_buf(tmpl.c_str(),
                                   tmpl.c_str() + tmpl.size() + 1);
        int fd = ::mkstemp(path_buf.data());
        if (fd < 0) {
            throw std::runtime_error(
                std::string("Failed to create temp file with prefix '") +
                std::string(prefix) + "': " + std::strerror(errno));
        }
        ::close(fd);
        path_ = fs::path(path_buf.data());
    }

    TempFile::~TempFile() {
        if (!path_.empty()) {
            std::error_code ec;
            fs::remove(path_, ec);
        }
    }

    TempFile::TempFile(TempFile&& other) noexcept
        : path_(std::move(other.path_)) {
        other.path_.clear();
    }

    TempFile& TempFile::operator=(TempFile&& other) noexcept {
        if (this != &other) {
            if (!path_.empty()) {
                std::error_code ec;
                fs::remove(path_, ec);
            }
            path_ = std::move(other.path_);
            other.path_.clear();
        }
        return *this;
    }

    void TempFile::write(const char* data, std::size_t len) {
        std::ofstream f(path_, std::ios::binary | std::ios::trunc);
        if (!f) {
            throw std::runtime_error("Failed to open temp file " +
                                     path_.string() + " for writing");
        }
        f.write(data, static_cast<std::streamsize>(len));
        if (!f) {
            throw std::runtime_error("Failed to write to temp file " +
                                     path_.string());
        }
    }

    std::optional<std::string> getenv_nonempty(const char* name) {
        const char* v = std::getenv(name);
        if (!v || *v == '\0') {
            return std::nullopt;
        }
        return std::string(v);
    }

    void spawn_and_wait(const std::string& program,
                        const std::vector<std::string>& args,
                        const std::vector<std::string>& extra_env) {
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        // Copy parent env, skipping any keys overridden by extra_env, then
        // append the overrides. Scoping the overrides to the child avoids
        // leaking them into the rest of this process.
        std::vector<std::string> env_storage;
        for (char** e = process_environ(); *e != nullptr; ++e) {
            std::string_view key = env_key(*e);
            bool overridden = false;
            for (const auto& extra : extra_env) {
                if (env_key(extra) == key) {
                    overridden = true;
                    break;
                }
            }
            if (!overridden) {
                env_storage.emplace_back(*e);
            }
        }
        for (const auto& extra : extra_env) {
            env_storage.push_back(extra);
        }
        std::vector<char*> envp;
        envp.reserve(env_storage.size() + 1);
        for (auto& s : env_storage) {
            envp.push_back(s.data());
        }
        envp.push_back(nullptr);

        pid_t pid = -1;
        int spawn_err = ::posix_spawnp(&pid, program.c_str(), nullptr, nullptr,
                                       argv.data(), envp.data());
        if (spawn_err != 0) {
            throw std::runtime_error("posix_spawnp(" + program +
                                     ") failed: " + std::strerror(spawn_err));
        }

        int status = 0;
        if (::waitpid(pid, &status, 0) < 0) {
            throw std::runtime_error("waitpid for " + program +
                                     " failed: " + std::strerror(errno));
        }
        if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
            throw std::runtime_error(program + " exited with non-zero status");
        }
    }

} // namespace hegel::impl::utils
