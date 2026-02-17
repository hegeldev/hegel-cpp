#pragma once

#include <cstdint>
#include <hegel/options.h>
#include <string>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl {
    class Connection;
}

namespace hegel::impl::socket {
    void set_embedded_connection(Connection* conn, uint32_t data_channel,
                                 options::Verbosity verbosity);
    void clear_embedded_connection();
    Connection* get_embedded_connection();
    uint32_t get_embedded_data_channel();
    bool is_test_aborted();
    void set_test_aborted(bool v);

    /// Wait for a socket file to appear on disk
    bool wait_for_socket(const std::string& path, int timeout_ms);

    /// Connect to a Unix domain socket, returning the file descriptor
    int connect_to_socket(const std::string& path);
} // namespace hegel::impl::socket
