#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace hegel::impl::uv {

    /// Returns the path to a `uv` binary.
    ///
    /// Lookup order:
    /// 1. `uv` found on `PATH`
    /// 2. Cached binary at `~/.cache/hegel/uv`
    /// 3. Installs uv to `~/.cache/hegel/uv` using the embedded installer
    ///    script
    ///
    /// Throws std::runtime_error if uv cannot be found or installed.
    std::string find_uv();

    /// Returns the hegel cache directory — `$XDG_CACHE_HOME/hegel` if set,
    /// otherwise `$HOME/.cache/hegel`. Throws if neither env var is set.
    std::filesystem::path cache_dir();

    // Test hooks ----------------------------------------------------------

    std::string find_uv_impl(std::optional<std::string> uv_in_path,
                             const std::filesystem::path& cache);

    std::filesystem::path
    cache_dir_from(std::optional<std::string> xdg_cache_home,
                   std::optional<std::string> home_dir);

    void install_uv_with_sh(const std::filesystem::path& cache,
                            const std::string& sh);

} // namespace hegel::impl::uv
