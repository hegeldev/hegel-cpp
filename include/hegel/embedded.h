/**
 * @file embedded.h
 * @brief Embedded mode implementation for Hegel SDK
 *
 * Contains the hegel() function template for running property-based tests
 * with Hegel as a subprocess.
 */

#ifndef HEGEL_EMBEDDED_HPP
#define HEGEL_EMBEDDED_HPP

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// POSIX headers
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <functional>

#include <nlohmann/json.hpp>

#include "core.h"
#include "detail.h"

namespace hegel {

/**
 * @brief Run property-based tests using Hegel in embedded mode.
 *
 * This is the recommended way to run property tests. The function:
 * 1. Creates a Unix socket server for communication
 * 2. Spawns the Hegel CLI as a subprocess
 * 3. Runs the test function for each generated test case
 * 4. Handles shrinking when failures occur
 * 5. Throws std::runtime_error if any test case fails
 *
 * @code{.cpp}
 * #include "hegel/hegel.h"
 *
 * int main() {
 *     hegel::hegel([]() {
 *         using namespace hegel::strategies;
 *         auto x = integers<int>({.min_value = 0, .max_value = 100}).generate();
 *         auto y = integers<int>({.min_value = 0, .max_value = 100}).generate();
 *
 *         // Property: x + y >= x (true for non-negative integers)
 *         if (x + y < x) {
 *             throw std::runtime_error("Addition underflow!");
 *         }
 *     }, {.test_cases = 1000});
 *
 *     return 0;
 * }
 * @endcode
 *
 * @tparam F Test function type (callable with no arguments)
 * @param test_fn The test function to run repeatedly
 * @param options Configuration options (test count, debug mode, hegel path)
 * @throws std::runtime_error if any test case fails
 * @see HegelOptions for configuration options
 */
void hegel(std::function<void()> test_fn, HegelOptions options = {});

}  // namespace hegel

#endif  // HEGEL_EMBEDDED_HPP
