/**
 * @file detail.hpp
 * @brief Internal implementation details for Hegel SDK
 *
 * Contains socket communication, connection management, and other
 * internal functions. Not part of the public API.
 */

#ifndef HEGEL_DETAIL_HPP
#define HEGEL_DETAIL_HPP

#include <nlohmann/json.hpp>
#include <string>

#include "core.hpp"

namespace hegel {
namespace detail {

// =============================================================================
// Socket Communication
// =============================================================================

std::string communicate_with_socket(const std::string& schema);

// =============================================================================
// Helpers
// =============================================================================

void set_is_last_run(bool v);
void set_embedded_connection(int fd);
void clear_embedded_connection();
std::string read_line(int fd);
void write_line(int fd, const std::string& line);

}  // namespace detail
}  // namespace hegel

#endif  // HEGEL_DETAIL_HPP
