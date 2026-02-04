#pragma once

#include <string>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
    void set_embedded_connection(int fd);
    void clear_embedded_connection();
    std::string read_line(int fd);
    void write_line(int fd, const std::string& line);
}  // namespace hegel::impl
