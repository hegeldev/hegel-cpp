#pragma once

/**
 * @file detail.h
 * @brief Internal implementation details for Hegel SDK
 *
 * Contains socket communication, connection management, and other
 * internal functions. Not part of the public API.
 */

#include <string>

namespace hegel::detail {
    std::string communicate_with_socket(const std::string& schema);
}  // namespace hegel::detail
