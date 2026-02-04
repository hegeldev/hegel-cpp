#pragma once

/**
 * @file internal.h
 * @brief Internal implementation details for Hegel SDK
 *
 * Contains internal parts of the Hegel SDK that are referenced
 * in any of the header files shipped in the public API.
 *
 * Constructs that are not used in any of the header files,
 * i.e., are completely internal, are part of hegel::impl
 * and exist only in the src/ directory.
 */

#include <string>

namespace hegel::internal {
    std::string communicate_with_socket(const std::string& schema);

    /* Print a note message for debugging.
     *
     * Only prints on the last run (final replay for counterexample output)
     * to avoid cluttering output during the many test iterations.
    */
    void note(const std::string& message);
}
