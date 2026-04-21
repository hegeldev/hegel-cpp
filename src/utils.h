#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hegel::impl::utils {

    /// Search PATH for a bare command name. Returns an absolute path if found.
    std::optional<std::string> which(const std::string& name);

    /// Throw std::runtime_error if `path` exists but is not executable.
    void validate_executable(const std::string& path);

    /// Return the env value of `name` if it is set and non-empty; else nullopt.
    std::optional<std::string> getenv_nonempty(const char* name);

    /// Spawn `program` (resolved via PATH) with `args` as argv, inheriting the
    /// parent environment but replacing any keys listed in `extra_env` with
    /// those entries (each "KEY=VALUE"). Waits for the child to exit. Throws
    /// std::runtime_error on spawn failure, waitpid failure, or non-zero exit.
    void spawn_and_wait(const std::string& program,
                        const std::vector<std::string>& args,
                        const std::vector<std::string>& extra_env = {});

    /// RAII handle for a unique temporary file under temp_directory_path().
    ///
    /// The ctor creates `<tempdir>/<prefix>-XXXXXX` via mkstemp and closes the
    /// returned fd; the dtor unlinks the file (errors swallowed). Move-only.
    class TempFile {
      public:
        explicit TempFile(std::string_view prefix);
        ~TempFile();

        TempFile(const TempFile&) = delete;
        TempFile& operator=(const TempFile&) = delete;
        TempFile(TempFile&& other) noexcept;
        TempFile& operator=(TempFile&& other) noexcept;

        const std::filesystem::path& path() const { return path_; }

        /// Overwrite the file with `len` bytes from `data` (binary,
        /// truncating).
        void write(const char* data, std::size_t len);

      private:
        std::filesystem::path path_;
    };

} // namespace hegel::impl::utils
