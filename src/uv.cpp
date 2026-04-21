#include "uv.h"

#include "utils.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// Defined in the generated uv_install_script.cpp.
extern const char UV_INSTALL_SCRIPT[];
extern const std::size_t UV_INSTALL_SCRIPT_LEN;

namespace hegel::impl::uv {

    fs::path cache_dir_from(std::optional<std::string> xdg_cache_home,
                            std::optional<std::string> home_dir) {
        if (xdg_cache_home.has_value()) {
            return fs::path(*xdg_cache_home) / "hegel";
        }
        if (!home_dir.has_value()) {
            throw std::runtime_error("Could not determine home directory");
        }
        return fs::path(*home_dir) / ".cache" / "hegel";
    }

    fs::path cache_dir() {
        return cache_dir_from(utils::getenv_nonempty("XDG_CACHE_HOME"),
                              utils::getenv_nonempty("HOME"));
    }

    void install_uv_with_sh(const fs::path& cache, const std::string& sh) {
        std::error_code ec;
        fs::create_directories(cache, ec);
        if (ec) {
            throw std::runtime_error("Failed to create cache directory " +
                                     cache.string() + ": " + ec.message());
        }

        // Materialise the embedded installer to a unique temp file and
        // hand sh a path. TempFile unlinks it on scope exit.
        utils::TempFile script("hegel-uv-install");
        script.write(UV_INSTALL_SCRIPT, UV_INSTALL_SCRIPT_LEN);

        utils::spawn_and_wait(sh, {sh, script.path().string()},
                              {"UV_UNMANAGED_INSTALL=" + cache.string()});
    }

    std::string find_uv_impl(std::optional<std::string> uv_in_path,
                             const fs::path& cache) {
        if (uv_in_path.has_value()) {
            return *uv_in_path;
        }
        fs::path cached = cache / "uv";
        std::error_code ec;
        if (fs::is_regular_file(cached, ec)) {
            return cached.string();
        }
        install_uv_with_sh(cache, "sh");
        return cached.string();
    }

    std::string find_uv() {
        return find_uv_impl(utils::which("uv"), cache_dir());
    }

} // namespace hegel::impl::uv
