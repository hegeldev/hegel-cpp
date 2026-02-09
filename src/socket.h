#pragma once

#include <cstdint>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl {
class Connection;
}

namespace hegel::impl::socket {
void set_embedded_connection(Connection* conn, uint32_t data_channel);
void clear_embedded_connection();
Connection* get_embedded_connection();
uint32_t get_embedded_data_channel();
bool is_test_aborted();
void set_test_aborted(bool v);
} // namespace hegel::impl::socket
