/*
 * hegel.cpp - Implementation of non-template functions
 */

#include <hegel/core.hpp>
#include <hegel/detail.hpp>
#include <hegel/detail/base64.hpp>
#include <hegel/generators.hpp>
#include <hegel/grouping.hpp>
#include <hegel/strategies.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

// POSIX headers
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace hegel {

// =============================================================================
// Thread-local Mode State
// =============================================================================

namespace detail {

static thread_local Mode current_mode_ = Mode::External;
static thread_local bool is_last_run_ = false;
static thread_local int embedded_fd_ = -1;
static thread_local std::string embedded_read_buffer_;

void set_mode(Mode m) { current_mode_ = m; }
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

Mode current_mode() { return detail::current_mode_; }
bool is_last_run() { return detail::is_last_run_; }

void note(const std::string& message) {
  if (detail::current_mode_ == Mode::External) {
    std::cerr << message << std::endl;
  } else if (detail::is_last_run_) {
    std::cerr << message << std::endl;
  }
  // In embedded mode on non-last runs: silently ignore
}

// =============================================================================
// Core functions
// =============================================================================

[[noreturn]] void stop_test() {
  if (detail::current_mode_ == Mode::Embedded) {
    throw HegelReject();
  }
  std::exit(detail::get_reject_code());
}

void assume(bool condition) {
  if (!condition) {
    stop_test();
  }
}

namespace detail {

int get_reject_code() {
  const char* env = std::getenv("HEGEL_REJECT_CODE");
  if (!env) {
    std::cerr << "hegel: HEGEL_REJECT_CODE environment variable not set\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }
  try {
    return std::stoi(env);
  } catch (...) {
    std::cerr << "hegel: HEGEL_REJECT_CODE is not a valid integer\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }
}

std::string get_socket_path() {
  const char* env = std::getenv("HEGEL_SOCKET");
  if (!env) {
    std::cerr << "hegel: HEGEL_SOCKET environment variable not set\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }
  return std::string(env);
}

// =============================================================================
// Thread-local connection state for persistent connections
// =============================================================================

struct ConnectionState {
  int socket_fd = -1;
  size_t span_depth = 0;
  std::string read_buffer;
};

// Thread-local connection state
static thread_local ConnectionState connection_state;
static thread_local int request_counter = 0;

bool is_connected() { return connection_state.socket_fd >= 0; }

size_t get_span_depth() { return connection_state.span_depth; }

void open_connection() {
  if (is_connected()) {
    std::cerr << "hegel: open_connection called when already connected\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  std::string socket_path = get_socket_path();

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    std::cerr << "hegel: failed to create socket\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (socket_path.size() >= sizeof(addr.sun_path)) {
    close(sock);
    std::cerr << "hegel: socket path too long\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }
  std::copy(socket_path.begin(), socket_path.end(), addr.sun_path);

  if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(sock);
    std::cerr << "hegel: failed to connect to socket at " << socket_path << "\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  connection_state.socket_fd = sock;
  connection_state.read_buffer.clear();
}

void close_connection() {
  if (!is_connected()) {
    std::cerr << "hegel: close_connection called when not connected\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  if (connection_state.span_depth != 0) {
    std::cerr << "hegel: close_connection called with " << connection_state.span_depth
              << " unclosed span(s)\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  close(connection_state.socket_fd);
  connection_state.socket_fd = -1;
  connection_state.read_buffer.clear();
}

void increment_span_depth() { connection_state.span_depth++; }

void decrement_span_depth() {
  if (connection_state.span_depth == 0) {
    std::cerr << "hegel: stop_span called with no open spans\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }
  connection_state.span_depth--;
}

nlohmann::json send_request(const std::string& command, const nlohmann::json& payload) {
  if (connection_state.socket_fd < 0) {
    std::cerr << "hegel: send_request called without active connection\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  int sock = connection_state.socket_fd;

  // Build the request
  int request_id = ++request_counter;
  nlohmann::json request = {
      {"id", request_id}, {"command", command}, {"payload", payload}};
  std::string message = request.dump() + "\n";

  // Debug logging if HEGEL_DEBUG is set
  if (std::getenv("HEGEL_DEBUG")) {
    std::cerr << "REQUEST: " << message;
  }

  // Send the message
  size_t total_sent = 0;
  while (total_sent < message.size()) {
    ssize_t sent =
        write(sock, message.data() + total_sent, message.size() - total_sent);
    if (sent < 0) {
      std::cerr << "hegel: failed to write to socket\n";
      std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }
    total_sent += static_cast<size_t>(sent);
  }

  // Read the response until newline (checking buffer first)
  std::string& buffer = connection_state.read_buffer;
  while (buffer.find('\n') == std::string::npos) {
    char read_buf[4096];
    ssize_t n = read(sock, read_buf, sizeof(read_buf));
    if (n < 0) {
      std::cerr << "hegel: failed to read from socket\n";
      std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }
    if (n == 0) {
      std::cerr << "hegel: connection closed unexpectedly\n";
      std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }
    buffer.append(read_buf, static_cast<size_t>(n));
  }

  // Extract the first line from buffer
  size_t newline_pos = buffer.find('\n');
  std::string response = buffer.substr(0, newline_pos);
  buffer.erase(0, newline_pos + 1);

  // Debug logging if HEGEL_DEBUG is set
  if (std::getenv("HEGEL_DEBUG")) {
    std::cerr << "RESPONSE: " << response << "\n";
  }

  // Parse and validate response
  nlohmann::json parsed = nlohmann::json::parse(response);

  // Verify request ID matches
  if (!parsed.contains("id") || parsed["id"] != request_id) {
    std::cerr << "hegel: response ID mismatch\n";
    std::exit(ASSERTION_FAILURE_EXIT_CODE);
  }

  return parsed;
}

std::string communicate_with_socket(const std::string& schema) {
  nlohmann::json payload = nlohmann::json::parse(schema);

  // In embedded mode, use the embedded connection directly
  if (current_mode_ == Mode::Embedded) {
    int fd = get_embedded_fd();
    if (fd < 0) {
      std::cerr << "hegel: embedded mode but no connection set\n";
      std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }

    // Build and send request
    static thread_local int embedded_request_counter = 0;
    int request_id = ++embedded_request_counter;
    nlohmann::json request = {
        {"id", request_id}, {"command", "generate"}, {"payload", payload}};
    std::string message = request.dump() + "\n";

    if (std::getenv("HEGEL_DEBUG")) {
      std::cerr << "REQUEST: " << message;
    }

    // Send
    size_t total_sent = 0;
    while (total_sent < message.size()) {
      ssize_t sent =
          write(fd, message.data() + total_sent, message.size() - total_sent);
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

  // External mode: manage connection locally
  bool need_connection = !is_connected();
  if (need_connection) {
    open_connection();
  }

  nlohmann::json response = send_request("generate", payload);

  if (need_connection) {
    close_connection();
  }

  return response.dump();
}

}  // namespace detail

// =============================================================================
// Span management
// =============================================================================

void start_span(uint64_t label) {
  // Open connection on first span
  if (!detail::is_connected()) {
    detail::open_connection();
  }
  detail::increment_span_depth();
  detail::send_request("start_span", {{"label", label}});
}

void stop_span(bool discard) {
  detail::decrement_span_depth();
  detail::send_request("stop_span", {{"discard", discard}});
  // Close connection when last span ends

  if (detail::get_span_depth() == 0) {
    detail::close_connection();
  }
}

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
  nlohmann::json schema = {{"type", "string"}};

  if (params.min_size > 0) schema["min_size"] = params.min_size;
  if (params.max_size) schema["max_size"] = *params.max_size;

  return from_schema<std::string>(schema.dump());
}

Generator<std::vector<uint8_t>> binary(BinaryParams params) {
  nlohmann::json schema = {{"type", "binary"}};

  if (params.min_size > 0) schema["min_size"] = params.min_size;
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
  nlohmann::json schema = {{"type", "regex"}, {"pattern", pattern}};
  if (fullmatch) {
    schema["fullmatch"] = true;
  }
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

}  // namespace hegel
