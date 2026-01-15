/**
 * @file detail.hpp
 * @brief Internal implementation details for Hegel SDK
 *
 * Contains socket communication, connection management, and other
 * internal functions. Not part of the public API.
 */

#ifndef HEGEL_DETAIL_HPP
#define HEGEL_DETAIL_HPP

#include <string>

#include <nlohmann/json.hpp>

#include "core.hpp"

namespace hegel {
namespace detail {

// =============================================================================
// Environment Variable Access
// =============================================================================

int get_reject_code();
std::string get_socket_path();

// =============================================================================
// Connection Lifecycle Management
// =============================================================================

bool is_connected();
void open_connection();
void close_connection();
size_t get_span_depth();
void increment_span_depth();
void decrement_span_depth();

// =============================================================================
// Socket Communication
// =============================================================================

nlohmann::json send_request(const std::string& command, const nlohmann::json& payload);
std::string communicate_with_socket(const std::string& schema);

// =============================================================================
// Embedded Mode Helpers
// =============================================================================

void set_mode(Mode m);
void set_is_last_run(bool v);
void set_embedded_connection(int fd);
void clear_embedded_connection();
std::string read_line(int fd);
void write_line(int fd, const std::string& line);

}  // namespace detail
}  // namespace hegel

#endif  // HEGEL_DETAIL_HPP
