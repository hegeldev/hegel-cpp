#include "utils.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <system_error>

namespace fs = std::filesystem;

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

} // namespace hegel::impl::utils
