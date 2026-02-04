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
    // =============================================================================
    // State
    // =============================================================================

    /**
    * @brief Check if this is the last test run.
    *
    * Useful in embedded mode to only print output once, after shrinking
    * is complete and the final failing case has been found.
    *
    * @return true if this is the last (possibly failing) run
    */
    bool is_last_run();

    void set_is_last_run(bool v);

    // =============================================================================
    // Socket Communication
    // =============================================================================

    std::string communicate_with_socket(const std::string& schema);
    void set_embedded_connection(int fd);
    void clear_embedded_connection();
    std::string read_line(int fd);
    void write_line(int fd, const std::string& line);

}  // namespace hegel::detail
