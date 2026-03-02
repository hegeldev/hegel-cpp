#pragma once

#include <string>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
    /// Wait for a socket file to appear on disk
    bool wait_for_socket(const std::string& path, int timeout_ms);

    /// Connect to a Unix domain socket, returning the file descriptor
    int connect_to_socket(const std::string& path);
} // namespace hegel::impl::socket
