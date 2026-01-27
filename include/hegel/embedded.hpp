/**
 * @file embedded.hpp
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

#include <nlohmann/json.hpp>

#include "core.hpp"
#include "detail.hpp"

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

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
 * #include "hegel/hegel.hpp"
 *
 * int main() {
 *     hegel::hegel([]() {
 *         using namespace hegel::st;
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
template <typename F>
void hegel(F&& test_fn, HegelOptions options = {}) {
  // Create temp directory with socket
  std::string temp_dir = "/tmp/hegel_" + std::to_string(getpid());
  std::filesystem::create_directories(temp_dir);
  std::string socket_path = temp_dir + "/hegel.sock";

  // Create server socket
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::copy(socket_path.begin(), socket_path.end(), addr.sun_path);

  if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(server_fd);
    throw std::runtime_error("Failed to bind socket");
  }

  if (listen(server_fd, 5) < 0) {
    close(server_fd);
    throw std::runtime_error("Failed to listen on socket");
  }

  // Build hegel command
  std::string hegel_path = options.hegel_path.value_or(HEGEL_DEFAULT_PATH);
  uint64_t test_cases = options.test_cases.value_or(100);
  std::vector<std::string> args = {
      hegel_path,     "--client-mode",
      socket_path,    "--no-tui",
      "--verbosity",  verbosity_to_string(options.verbosity),
      "--test-cases", std::to_string(test_cases)};

  // Fork and exec hegel
  pid_t pid = fork();
  if (pid < 0) {
    close(server_fd);
    throw std::runtime_error("Failed to fork");
  }

  if (pid == 0) {
    // Child: exec hegel
    std::vector<char*> argv;
    for (auto& a : args) {
      argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    std::exit(1);
  }

  // Parent: accept connections until hegel exits
  fd_set fds;
  struct timeval tv;

  while (true) {
    FD_ZERO(&fds);
    FD_SET(server_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms

    int ready = select(server_fd + 1, &fds, nullptr, nullptr, &tv);

    if (ready > 0) {
      int client_fd = accept(server_fd, nullptr, nullptr);
      if (client_fd >= 0) {
        // Handle connection
        try {
          // Read handshake
          std::string line = detail::read_line(client_fd);
          auto handshake = nlohmann::json::parse(line);
          bool is_last = handshake.value("is_last_run", false);

          // Set thread-local state
          detail::set_is_last_run(is_last);
          detail::set_embedded_connection(client_fd);

          // Send handshake_ack
          detail::write_line(client_fd, R"({"type": "handshake_ack"})");

          // Run test
          std::string result_type = "pass";
          std::string error_message;
          try {
            test_fn();
          } catch (const HegelReject& e) {
            result_type = "reject";
            error_message = e.what();
          } catch (const std::exception& e) {
            result_type = "fail";
            error_message = e.what();
          } catch (...) {
            result_type = "fail";
            error_message = "Unknown exception";
          }

          // Clear embedded connection
          detail::clear_embedded_connection();

          // Send result
          nlohmann::json result = {{"type", "test_result"}, {"result", result_type}};
          if (!error_message.empty()) {
            result["message"] = error_message;
          }
          detail::write_line(client_fd, result.dump());
        } catch (...) {
          detail::clear_embedded_connection();
        }
        close(client_fd);
      }
    }

    // Check if hegel exited
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        close(server_fd);
        std::filesystem::remove_all(temp_dir);
        throw std::runtime_error("Hegel test failed (exit code " +
                                 std::to_string(WEXITSTATUS(status)) + ")");
      }
      break;
    }
  }

  // Cleanup
  close(server_fd);
  std::filesystem::remove_all(temp_dir);
}

}  // namespace hegel

#endif  // HEGEL_EMBEDDED_HPP
