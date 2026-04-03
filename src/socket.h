#pragma once

#include <string>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
    /// Connect to a Unix domain socket, retrying until timeout_ms expires.
    int connect_to_socket(const std::string& path, int timeout_ms);
} // namespace hegel::impl::socket
