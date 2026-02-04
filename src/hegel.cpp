/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <functional>
#include <hegel/core.h>
#include <hegel/detail.h>
#include <hegel/detail/base64.h>
#include <hegel/generators.h>
#include <hegel/grouping.h>
#include <hegel/strategies.h>
#include <hegel/embedded.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

// POSIX headers
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

namespace hegel {

// =============================================================================
// Thread-local State
// =============================================================================

namespace detail {

static thread_local bool is_last_run_ = false;
static thread_local int embedded_fd_ = -1;
static thread_local std::string embedded_read_buffer_;

void set_is_last_run(bool v) { is_last_run_ = v; }

void set_embedded_connection(int fd) {
  embedded_fd_ = fd;
  embedded_read_buffer_.clear();
}

void clear_embedded_connection() {
  embedded_fd_ = -1;
  embedded_read_buffer_.clear();
}

int get_embedded_fd() { return embedded_fd_; }

std::string& get_embedded_read_buffer() { return embedded_read_buffer_; }

std::string read_line(int fd) {
  std::string line;
  char c;
  while (read(fd, &c, 1) == 1) {
    if (c == '\n') {
      break;
    }
    line += c;
  }
  return line;
}

void write_line(int fd, const std::string& line) {
  std::string msg = line + "\n";
  size_t total = 0;
  while (total < msg.size()) {
    // Use send() with MSG_NOSIGNAL to prevent SIGPIPE on closed socket
    ssize_t n = send(fd, msg.data() + total, msg.size() - total, MSG_NOSIGNAL);
    if (n < 0) {
      break;
    }
    total += static_cast<size_t>(n);
  }
}

}  // namespace detail

bool is_last_run() { return detail::is_last_run_; }

void note(const std::string& message) {
  if (detail::is_last_run_) {
    std::cerr << message << std::endl;
  }
}

// =============================================================================
// Core functions
// =============================================================================

[[noreturn]] void stop_test() { throw HegelReject(); }

void assume(bool condition) {
  if (!condition) {
    stop_test();
  }
}

namespace detail {

std::string communicate_with_socket(const std::string& schema) {
  nlohmann::json payload = nlohmann::json::parse(schema);

  int fd = get_embedded_fd();
  if (fd < 0) {
    std::cerr << "hegel: no connection set\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  // Build and send request
  static thread_local int request_counter = 0;
  int request_id = ++request_counter;
  nlohmann::json request = {
      {"id", request_id}, {"command", "generate"}, {"payload", payload}};
  std::string message = request.dump() + "\n";

  if (std::getenv("HEGEL_DEBUG")) {
    std::cerr << "REQUEST: " << message;
  }

  // Send
  size_t total_sent = 0;
  while (total_sent < message.size()) {
    ssize_t sent = write(fd, message.data() + total_sent, message.size() - total_sent);
    if (sent < 0) {
      std::cerr << "hegel: failed to write to socket\n";
      std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }
    total_sent += static_cast<size_t>(sent);
  }

  // Read response
  std::string& buffer = get_embedded_read_buffer();
  while (true) {
    size_t newline = buffer.find('\n');
    if (newline != std::string::npos) {
      std::string line = buffer.substr(0, newline);
      buffer.erase(0, newline + 1);

      if (std::getenv("HEGEL_DEBUG")) {
        std::cerr << "RESPONSE: " << line << "\n";
      }

      auto response = nlohmann::json::parse(line);
      if (response.contains("reject")) {
        // Rejection: hypothesis stopped this test case (e.g., buffer
        // exhausted). Just stop the test - hegel already knows it's a
        // rejection.
        stop_test();
      }
      if (response.contains("error")) {
        // Genuine error: bad schema, invalid request, etc.
        // Throw runtime_error to propagate as a real test failure.
        throw std::runtime_error("hegel: " + response["error"].get<std::string>());
      }
      // Auto-log generated value during final replay (counterexample)
      if (is_last_run_) {
        std::cerr << "Generated: " << response["result"].dump() << "\n";
      }
      // Return full response for parsing by Generator::generate_impl
      return response.dump();
    }

    char chunk[4096];
    ssize_t n = read(fd, chunk, sizeof(chunk));
    if (n <= 0) {
      std::cerr << "hegel: connection closed while reading response (n=" << n
                << ", errno=" << errno << ", fd=" << fd << ")\n";
      std::cerr << "hegel: buffer so far: " << buffer << "\n";
      std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }
    buffer.append(chunk, static_cast<size_t>(n));
  }
}

}  // namespace detail

// =============================================================================
// Strategy implementations
// =============================================================================

namespace st {

Generator<std::monostate> nulls() {
  nlohmann::json schema = {{"type", "null"}};
  return Generator<std::monostate>([]() { return std::monostate{}; }, schema.dump());
}

Generator<bool> booleans() {
  nlohmann::json schema = {{"type", "boolean"}};
  return from_schema<bool>(schema.dump());
}

Generator<std::string> text(TextParams params) {
  nlohmann::json schema = {{"type", "string"}, {"min_size", params.min_size}};

  if (params.max_size) schema["max_size"] = *params.max_size;

  return from_schema<std::string>(schema.dump());
}

Generator<std::vector<uint8_t>> binary(BinaryParams params) {
  nlohmann::json schema = {{"type", "binary"}, {"min_size", params.min_size}};

  if (params.max_size) schema["max_size"] = *params.max_size;

  std::string schema_str = schema.dump();
  return Generator<std::vector<uint8_t>>(
      [schema_str]() -> std::vector<uint8_t> {
        std::string b64 = from_schema<std::string>(schema_str).generate();
        return ::hegel::detail::base64_decode(b64);
      },
      schema_str);
}

Generator<std::string> from_regex(const std::string& pattern, bool fullmatch) {
  nlohmann::json schema = {
      {"type", "regex"}, {"pattern", pattern}, {"fullmatch", fullmatch}};
  return from_schema<std::string>(schema.dump());
}

Generator<std::string> emails() {
  return from_schema<std::string>(R"({"type": "email"})");
}

Generator<std::string> domains(DomainsParams params) {
  nlohmann::json schema = {{"type", "domain"}, {"max_length", params.max_length}};
  return from_schema<std::string>(schema.dump());
}

Generator<std::string> urls() { return from_schema<std::string>(R"({"type": "url"})"); }

Generator<std::string> ip_addresses(IpAddressesParams params) {
  if (params.v == 4) {
    return from_schema<std::string>(R"({"type": "ipv4"})");
  } else if (params.v == 6) {
    return from_schema<std::string>(R"({"type": "ipv6"})");
  } else {
    return from_schema<std::string>(
        R"({"one_of": [{"type": "ipv4"}, {"type": "ipv6"}]})");
  }
}

Generator<std::string> dates() {
  return from_schema<std::string>(R"({"type": "date"})");
}

Generator<std::string> times() {
  return from_schema<std::string>(R"({"type": "time"})");
}

Generator<std::string> datetimes() {
  return from_schema<std::string>(R"({"type": "datetime"})");
}

}  // namespace st


void hegel(std::function<void()> test_fn, HegelOptions options) {
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
