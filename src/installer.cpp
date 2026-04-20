#include "installer.h"

#include "utils.h"
#include "uv.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace hegel::impl {

    std::string resolve_hegel_path(const std::string& path) {
        std::error_code ec;
        if (fs::exists(path, ec)) {
            utils::validate_executable(path);
            return path;
        }

        // Bare name (no '/') — try PATH lookup.
        if (path.find('/') == std::string::npos) {
            if (auto resolved = utils::which(path); resolved.has_value()) {
                utils::validate_executable(*resolved);
                return *resolved;
            }
            throw std::runtime_error(
                "Hegel server binary '" + path +
                "' not found on PATH. Check that " + HEGEL_SERVER_COMMAND_ENV +
                " is set correctly, or install hegel-core.");
        }

        throw std::runtime_error("Hegel server binary not found at '" + path +
                                 "'. Check that " + HEGEL_SERVER_COMMAND_ENV +
                                 " is set correctly.");
    }

    std::vector<std::string> hegel_command() {
        if (auto override_path =
                utils::getenv_nonempty(HEGEL_SERVER_COMMAND_ENV);
            override_path.has_value()) {
            return {resolve_hegel_path(*override_path)};
        }

        std::string uv = uv::find_uv();
        return {uv,
                "tool",
                "run",
                "--from",
                std::string("hegel-core==") + HEGEL_SERVER_VERSION,
                "hegel"};
    }

} // namespace hegel::impl
