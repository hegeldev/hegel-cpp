#pragma once

#include <optional>
#include <string>

namespace hegel::impl::utils {

    /// Search PATH for a bare command name. Returns an absolute path if found.
    std::optional<std::string> which(const std::string& name);

    /// Throw std::runtime_error if `path` exists but is not executable.
    void validate_executable(const std::string& path);

} // namespace hegel::impl::utils
