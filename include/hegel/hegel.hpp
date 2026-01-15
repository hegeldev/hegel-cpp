/*
 * hegel.hpp - Type-safe generation via Unix sockets with Hypothesis-like
 * strategies
 *
 * Dependencies:
 *   - reflect-cpp (https://github.com/getml/reflect-cpp) - C++20
 *   - nlohmann/json (https://github.com/nlohmann/json) - for schema
 * manipulation
 *   - POSIX sockets (sys/socket.h, sys/un.h, unistd.h)
 *
 * Environment variables:
 *   - HEGEL_SOCKET: Path to the Unix socket for generation requests
 *   - HEGEL_REJECT_CODE: Exit code to use when reject() is called
 *   - HEGEL_DEBUG: If set, also print REQUEST/RESPONSE JSON to stderr
 *
 * Usage:
 *   // Type-based generation
 *   auto gen = hegel::default_generator<Person>();
 *   Person p = gen.generate();
 *
 *   // Strategy-based generation
 *   using namespace hegel::st;
 *   auto gen = vectors(integers<int>({.min_value = 0, .max_value = 100}),
 * {.min_size = 1}); std::vector<int> result = gen.generate();
 */

#ifndef HEGEL_HPP
#define HEGEL_HPP

#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// POSIX headers
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// C++ filesystem
#include <filesystem>

// reflect-cpp headers
#include <rfl.hpp>
#include <rfl/json.hpp>

// nlohmann/json for schema manipulation
#include <nlohmann/json.hpp>

// Default path to hegel binary (can be overridden by CMake)
#ifndef HEGEL_DEFAULT_PATH
#define HEGEL_DEFAULT_PATH "hegel"
#endif

namespace hegel {

// =============================================================================
// Constants
// =============================================================================

inline constexpr int ASSERTION_FAILURE_EXIT_CODE = 134;

// =============================================================================
// Mode and State
// =============================================================================

/// Execution mode for the Hegel SDK
enum class Mode {
  /// Standalone mode: test binary runs, hegel is external
  Standalone,
  /// Embedded mode: test binary runs hegel as subprocess
  Embedded
};

/// Get the current execution mode
Mode current_mode();

/// Check if this is the last run (for note() output in embedded mode)
bool is_last_run();

/// Print a note message.
/// In standalone mode, always prints to stderr.
/// In embedded mode, only prints on the last run.
void note(const std::string& message);

// =============================================================================
// Embedded Mode Options
// =============================================================================

struct HegelOptions {
  std::optional<uint64_t> test_cases;
  bool debug = false;
  std::optional<std::string> hegel_path;  // Default: "hegel"
};

// =============================================================================
// Embedded Mode Exception
// =============================================================================

/// Exception thrown by reject() in embedded mode
class HegelReject : public std::exception {
  std::string message_;

 public:
  explicit HegelReject(const std::string& msg) : message_(msg) {}
  const char* what() const noexcept override { return message_.c_str(); }
};

// =============================================================================
// Forward declarations
// =============================================================================

template <typename T>
class Generator;
template <typename T>
class DefaultGenerator;

// =============================================================================
// Non-template function declarations (implemented in hegel.cpp)
// =============================================================================

/// Stop the current test case immediately.
/// In embedded mode, throws HegelReject.
/// In standalone mode, exits with HEGEL_REJECT_CODE.
[[noreturn]] void stop_test();

/// Reject the current test case with a message.
/// Prints the message (in standalone mode or last run) and stops the test.
[[noreturn]] void reject(const std::string& message);

// =============================================================================
// Internal detail namespace
// =============================================================================

namespace detail {

int get_reject_code();
std::string get_socket_path();

// Connection lifecycle management
bool is_connected();
void open_connection();
void close_connection();
size_t get_span_depth();
void increment_span_depth();
void decrement_span_depth();
nlohmann::json send_request(const std::string& command, const nlohmann::json& payload);
std::string communicate_with_socket(const std::string& schema);

}  // namespace detail

// =============================================================================
// Grouped Generation
// =============================================================================

// Low-level span management (prefer group/discardable_group instead)
void start_span(uint64_t label);
void stop_span(bool discard = false);

/// Run a function within a labeled group.
///
/// Groups related generation calls together, which helps the testing engine
/// understand the structure of generated data and improve shrinking.
template <typename F>
auto group(uint64_t label, F&& f) -> decltype(f()) {
  start_span(label);
  auto result = f();
  stop_span(false);
  return result;
}

/// Run a function within a labeled group, discarding if it returns nullopt.
///
/// Useful for filter-like operations where rejected values should be discarded.
template <typename F>
auto discardable_group(uint64_t label, F&& f) -> decltype(f()) {
  start_span(label);
  auto result = f();
  stop_span(!result.has_value());
  return result;
}

namespace labels {
constexpr uint64_t LIST = 1;
constexpr uint64_t LIST_ELEMENT = 2;
constexpr uint64_t SET = 3;
constexpr uint64_t SET_ELEMENT = 4;
constexpr uint64_t MAP = 5;
constexpr uint64_t MAP_ENTRY = 6;
constexpr uint64_t TUPLE = 7;
constexpr uint64_t ONE_OF = 8;
constexpr uint64_t OPTIONAL = 9;
constexpr uint64_t FIXED_DICT = 10;
constexpr uint64_t FLAT_MAP = 11;
constexpr uint64_t FILTER = 12;
}  // namespace labels

// =============================================================================
// Response wrapper for socket communication
// =============================================================================

template <typename T>
struct Response {
  std::optional<T> result;
  std::optional<std::string> error;
};

// =============================================================================
// Generator class template
// =============================================================================

template <typename T>
class Generator {
 private:
  std::function<T()> gen_fn_;
  std::optional<std::string> schema_;

 public:
  explicit Generator(std::function<T()> fn) : gen_fn_(std::move(fn)) {}

  Generator(std::function<T()> fn, std::string schema)
      : gen_fn_(std::move(fn)), schema_(std::move(schema)) {}

  const std::optional<std::string>& schema() const { return schema_; }

  T generate() const { return gen_fn_(); }

  template <typename F>
  auto map(F&& f) const -> Generator<std::invoke_result_t<F, T>> {
    using U = std::invoke_result_t<F, T>;
    auto inner = gen_fn_;
    return Generator<U>([inner, f = std::forward<F>(f)]() -> U { return f(inner()); });
  }

  template <typename F>
  auto flatmap(F&& f) const -> std::invoke_result_t<F, T> {
    using ResultGen = std::invoke_result_t<F, T>;
    using U = decltype(std::declval<ResultGen>().generate());
    auto inner = gen_fn_;
    return Generator<U>(
        [inner, f = std::forward<F>(f)]() -> U { return f(inner()).generate(); });
  }

  Generator<T> filter(std::function<bool(const T&)> pred, int max_attempts = 3) const {
    auto inner = gen_fn_;
    return Generator<T>([inner, pred, max_attempts]() -> T {
      for (int i = 0; i < max_attempts; ++i) {
        T value = inner();
        if (pred(value)) {
          return value;
        }
      }
      reject("filter: failed to generate a value satisfying predicate after " +
             std::to_string(max_attempts) + " attempts");
    });
  }
};

// =============================================================================
// DefaultGenerator class template
// =============================================================================

template <typename T>
class DefaultGenerator {
 private:
  std::string schema_;

  T generate_impl() const {
    std::string response_json = detail::communicate_with_socket(schema_);

    auto parse_result = rfl::json::read<Response<T>>(response_json);
    if (!parse_result) {
      reject("hegel: failed to parse response: " + parse_result.error().what());
    }

    const Response<T>& response = parse_result.value();

    if (response.error) {
      reject(*response.error);
    }

    if (!response.result) {
      reject("hegel: response contained neither result nor error");
    }

    return *response.result;
  }

 public:
  DefaultGenerator() : schema_(rfl::json::to_schema<T>()) {}

  std::string& schema() { return schema_; }
  const std::string& schema() const { return schema_; }

  T generate() const { return generate_impl(); }

  Generator<T> to_generator() const {
    std::string schema_copy = schema_;
    return Generator<T>([schema_copy]() -> T {
      std::string response_json = detail::communicate_with_socket(schema_copy);

      auto parse_result = rfl::json::read<Response<T>>(response_json);
      if (!parse_result) {
        reject("hegel: failed to parse response: " + parse_result.error().what());
      }

      const Response<T>& response = parse_result.value();

      if (response.error) {
        reject(*response.error);
      }

      if (!response.result) {
        reject("hegel: response contained neither result nor error");
      }

      return *response.result;
    });
  }

  operator Generator<T>() const { return to_generator(); }

  template <typename F>
  auto map(F&& f) const -> Generator<std::invoke_result_t<F, T>> {
    return to_generator().map(std::forward<F>(f));
  }

  template <typename F>
  auto flatmap(F&& f) const -> std::invoke_result_t<F, T> {
    return to_generator().flatmap(std::forward<F>(f));
  }

  Generator<T> filter(std::function<bool(const T&)> pred) const {
    return to_generator().filter(std::move(pred));
  }
};

// =============================================================================
// Factory functions
// =============================================================================

template <typename T>
DefaultGenerator<T> default_generator() {
  return DefaultGenerator<T>();
}

template <typename T>
Generator<T> from_schema(std::string schema) {
  return Generator<T>(
      [schema]() -> T {
        std::string response_json = detail::communicate_with_socket(schema);

        auto parse_result = rfl::json::read<Response<T>>(response_json);
        if (!parse_result) {
          reject("hegel: failed to parse response: " + parse_result.error().what());
        }

        const Response<T>& response = parse_result.value();

        if (response.error) {
          reject(*response.error);
        }

        if (!response.result) {
          reject("hegel: response contained neither result nor error");
        }

        return *response.result;
      },
      schema);
}

// =============================================================================
// Strategies namespace
// =============================================================================

namespace st {

// =============================================================================
// Parameter structs
// =============================================================================

template <typename T>
struct IntegersParams {
  std::optional<T> min_value;
  std::optional<T> max_value;
};

template <typename T>
struct FloatsParams {
  std::optional<T> min_value;
  std::optional<T> max_value;
  bool exclude_min = false;
  bool exclude_max = false;
};

struct TextParams {
  size_t min_size = 0;
  std::optional<size_t> max_size;
};

struct DomainsParams {
  size_t max_length = 255;
};

struct IpAddressesParams {
  std::optional<int> v;  // 4 or 6, or nullopt for both
};

struct VectorsParams {
  size_t min_size = 0;
  std::optional<size_t> max_size;
  bool unique = false;
};

struct SetsParams {
  size_t min_size = 0;
  std::optional<size_t> max_size;
};

struct DictionariesParams {
  size_t min_size = 0;
  std::optional<size_t> max_size;
};

// =============================================================================
// Non-template strategy declarations (implemented in hegel.cpp)
// =============================================================================

Generator<std::monostate> nulls();
Generator<bool> booleans();
Generator<std::string> text(TextParams params = {});
Generator<std::string> from_regex(const std::string& pattern);
Generator<std::string> emails();
Generator<std::string> domains(DomainsParams params = {});
Generator<std::string> urls();
Generator<std::string> ip_addresses(IpAddressesParams params = {});
Generator<std::string> dates();
Generator<std::string> times();
Generator<std::string> datetimes();

// =============================================================================
// Template strategies (must be in header)
// =============================================================================

// just(value) - generates exactly this value
template <typename T>
Generator<T> just(T value) {
  nlohmann::json schema;

  if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::nullptr_t> ||
                std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
    schema = {{"const", value}};
    return Generator<T>([value]() { return value; }, schema.dump());
  } else {
    return Generator<T>([value]() { return value; });
  }
}

inline Generator<std::string> just(const char* value) {
  return just(std::string(value));
}

// integers<T>() - generates integers of type T
template <typename T = int64_t>
  requires std::is_integral_v<T>
Generator<T> integers(IntegersParams<T> params = {}) {
  nlohmann::json schema = {{"type", "integer"}};

  T min_val = params.min_value.value_or(std::numeric_limits<T>::min());
  T max_val = params.max_value.value_or(std::numeric_limits<T>::max());

  schema["minimum"] = min_val;
  schema["maximum"] = max_val;

  return from_schema<T>(schema.dump());
}

// floats<T>() - generates floating point numbers
template <typename T = double>
  requires std::is_floating_point_v<T>
Generator<T> floats(FloatsParams<T> params = {}) {
  nlohmann::json schema = {{"type", "number"}};

  if (params.min_value) {
    if (params.exclude_min) {
      schema["exclusiveMinimum"] = *params.min_value;
    } else {
      schema["minimum"] = *params.min_value;
    }
  }

  if (params.max_value) {
    if (params.exclude_max) {
      schema["exclusiveMaximum"] = *params.max_value;
    } else {
      schema["maximum"] = *params.max_value;
    }
  }

  return from_schema<T>(schema.dump());
}

// vectors(elements) - generates vectors
template <typename T>
Generator<std::vector<T>> vectors(Generator<T> elements, VectorsParams params = {}) {
  if (elements.schema()) {
    nlohmann::json elem_schema = nlohmann::json::parse(*elements.schema());
    nlohmann::json schema = {{"type", "array"}, {"items", elem_schema}};

    if (params.min_size > 0) schema["minItems"] = params.min_size;
    if (params.max_size) schema["maxItems"] = *params.max_size;
    if (params.unique) schema["uniqueItems"] = true;

    return from_schema<std::vector<T>>(schema.dump());
  }

  size_t max_size = params.max_size.value_or(100);

  auto length_gen =
      integers<size_t>({.min_value = params.min_size, .max_value = max_size});

  return Generator<std::vector<T>>([elements, length_gen]() {
    size_t len = length_gen.generate();
    std::vector<T> result;
    result.reserve(len);

    for (size_t i = 0; i < len; ++i) {
      result.push_back(elements.generate());
    }

    return result;
  });
}

// sets(elements) - generates sets
template <typename T>
Generator<std::set<T>> sets(Generator<T> elements, SetsParams params = {}) {
  if (elements.schema()) {
    nlohmann::json elem_schema = nlohmann::json::parse(*elements.schema());
    nlohmann::json schema = {
        {"type", "array"}, {"items", elem_schema}, {"uniqueItems", true}};

    if (params.min_size > 0) schema["minItems"] = params.min_size;
    if (params.max_size) schema["maxItems"] = *params.max_size;

    auto vec_gen = from_schema<std::vector<T>>(schema.dump());

    return Generator<std::set<T>>([vec_gen]() {
      auto vec = vec_gen.generate();
      return std::set<T>(vec.begin(), vec.end());
    });
  }

  size_t max_size = params.max_size.value_or(20);
  auto length_gen =
      integers<size_t>({.min_value = params.min_size, .max_value = max_size});

  return Generator<std::set<T>>([elements, length_gen]() {
    size_t target_len = length_gen.generate();
    std::set<T> result;

    size_t attempts = 0;
    while (result.size() < target_len && attempts < target_len * 10) {
      result.insert(elements.generate());
      ++attempts;
    }

    return result;
  });
}

// dictionaries(keys, values) - generates maps with string keys
template <typename K, typename V>
  requires std::is_same_v<K, std::string>
Generator<std::map<K, V>> dictionaries(Generator<K> keys, Generator<V> values,
                                       DictionariesParams params = {}) {
  if (values.schema()) {
    nlohmann::json schema = {
        {"type", "object"},
        {"additionalProperties", nlohmann::json::parse(*values.schema())}};

    if (params.min_size > 0) schema["minProperties"] = params.min_size;
    if (params.max_size) schema["maxProperties"] = *params.max_size;

    return from_schema<std::map<K, V>>(schema.dump());
  }

  size_t max_size = params.max_size.value_or(20);
  auto length_gen =
      integers<size_t>({.min_value = params.min_size, .max_value = max_size});

  return Generator<std::map<K, V>>([keys, values, length_gen]() {
    size_t len = length_gen.generate();
    std::map<K, V> result;

    while (result.size() < len) {
      K key = keys.generate();
      V value = values.generate();
      result[std::move(key)] = std::move(value);
    }

    return result;
  });
}

// tuples(gens...) - generates tuples
namespace detail {

template <typename... Gens>
auto make_tuple_schema(const Gens&... gens) -> std::optional<std::string> {
  bool all_have_schemas = (gens.schema().has_value() && ...);
  if (!all_have_schemas) return std::nullopt;

  nlohmann::json prefix_items = nlohmann::json::array();
  (prefix_items.push_back(nlohmann::json::parse(*gens.schema())), ...);

  nlohmann::json schema = {{"type", "array"},
                           {"prefixItems", prefix_items},
                           {"items", false},
                           {"minItems", sizeof...(Gens)},
                           {"maxItems", sizeof...(Gens)}};

  return schema.dump();
}

template <typename Tuple, typename GenTuple, size_t... Is>
Tuple generate_tuple_impl(const GenTuple& gens, std::index_sequence<Is...>) {
  return Tuple{std::get<Is>(gens).generate()...};
}

}  // namespace detail

template <typename... Ts>
Generator<std::tuple<Ts...>> tuples(Generator<Ts>... gens) {
  using ResultTuple = std::tuple<Ts...>;

  auto maybe_schema = detail::make_tuple_schema(gens...);

  if (maybe_schema) {
    return from_schema<ResultTuple>(*maybe_schema);
  }

  auto gen_tuple = std::make_tuple(std::move(gens)...);

  return Generator<ResultTuple>([gen_tuple]() {
    return detail::generate_tuple_impl<ResultTuple>(gen_tuple,
                                                    std::index_sequence_for<Ts...>{});
  });
}

// sampled_from(elements) - picks from a fixed set
template <typename T>
Generator<T> sampled_from(std::vector<T> elements) {
  if (elements.empty()) {
    reject("sampled_from: cannot sample from empty collection");
  }

  if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::nullptr_t> ||
                std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
    nlohmann::json schema = {{"enum", elements}};
    return from_schema<T>(schema.dump());
  } else {
    auto index_gen =
        integers<size_t>({.min_value = 0, .max_value = elements.size() - 1});

    return Generator<T>([elements, index_gen]() {
      size_t idx = index_gen.generate();
      return elements[idx];
    });
  }
}

template <typename T>
Generator<T> sampled_from(std::initializer_list<T> elements) {
  return sampled_from(std::vector<T>(elements));
}

inline Generator<std::string> sampled_from(
    std::initializer_list<const char*> elements) {
  std::vector<std::string> strings;
  strings.reserve(elements.size());
  for (const char* s : elements) {
    strings.push_back(s);
  }
  return sampled_from(std::move(strings));
}

// one_of(gens) - chooses from homogeneous generators
namespace detail {

template <typename T>
auto make_one_of_schema(const std::vector<Generator<T>>& gens)
    -> std::optional<std::string> {
  for (const auto& gen : gens) {
    if (!gen.schema()) return std::nullopt;
  }

  nlohmann::json any_of = nlohmann::json::array();
  for (const auto& gen : gens) {
    any_of.push_back(nlohmann::json::parse(*gen.schema()));
  }

  nlohmann::json schema = {{"anyOf", any_of}};
  return schema.dump();
}

}  // namespace detail

template <typename T>
Generator<T> one_of(std::vector<Generator<T>> gens) {
  if (gens.empty()) {
    reject("one_of: cannot choose from empty collection");
  }

  auto maybe_schema = detail::make_one_of_schema(gens);

  if (maybe_schema) {
    return from_schema<T>(*maybe_schema);
  }

  auto index_gen = integers<size_t>({.min_value = 0, .max_value = gens.size() - 1});

  return Generator<T>([gens, index_gen]() {
    size_t idx = index_gen.generate();
    return gens[idx].generate();
  });
}

template <typename T>
Generator<T> one_of(std::initializer_list<Generator<T>> gens) {
  return one_of(std::vector<Generator<T>>(gens));
}

// variant_(gens...) - generates std::variant from heterogeneous generators
namespace detail {

template <typename Variant, typename GenTuple, size_t I = 0>
Variant generate_variant_impl(const GenTuple& gens, size_t idx) {
  if constexpr (I < std::tuple_size_v<GenTuple>) {
    if (idx == I) {
      return std::get<I>(gens).generate();
    }
    return generate_variant_impl<Variant, GenTuple, I + 1>(gens, idx);
  } else {
    return Variant{};
  }
}

}  // namespace detail

template <typename... Ts>
Generator<std::variant<Ts...>> variant_(Generator<Ts>... gens) {
  using ResultVariant = std::variant<Ts...>;
  constexpr size_t N = sizeof...(Ts);

  auto gen_tuple = std::make_tuple(std::move(gens)...);
  auto index_gen = integers<size_t>({.min_value = 0, .max_value = N - 1});

  return Generator<ResultVariant>([gen_tuple, index_gen]() {
    size_t idx = index_gen.generate();
    return detail::generate_variant_impl<ResultVariant, decltype(gen_tuple)>(gen_tuple,
                                                                             idx);
  });
}

// optional_(gen) - generates std::optional<T>
template <typename T>
Generator<std::optional<T>> optional_(Generator<T> gen) {
  auto bool_gen = booleans();

  return Generator<std::optional<T>>([gen, bool_gen]() -> std::optional<T> {
    bool is_none = bool_gen.generate();
    if (is_none) {
      return std::nullopt;
    }
    return gen.generate();
  });
}

// builds<T>(gens...) - constructs T from generators (positional)
template <typename T, typename... Gens>
Generator<T> builds(Gens... gens) {
  auto gen_tuple = std::make_tuple(std::move(gens)...);

  return Generator<T>([gen_tuple]() {
    return std::apply([](auto&... g) { return T(g.generate()...); }, gen_tuple);
  });
}

// field<&T::member>(gen) - for named aggregate initialization
template <auto MemberPtr, typename Gen>
struct Field {
  static constexpr auto member_ptr = MemberPtr;
  Gen generator;
};

template <auto MemberPtr, typename Gen>
Field<MemberPtr, Gen> field(Gen gen) {
  return Field<MemberPtr, Gen>{std::move(gen)};
}

// builds_agg<T>(fields...) - aggregate initialization with named fields
// Note: We deduce Fields... directly rather than separately deducing MemberPtrs
// and Gens to avoid Clang 21 libc++ issues with auto... NTTP deduction.
template <typename T, typename... Fields>
Generator<T> builds_agg(Fields... fields) {
  auto gen_tuple = std::make_tuple(std::move(fields)...);

  return Generator<T>([gen_tuple]() mutable {
    T result{};
    std::apply(
        [&result](auto&... fs) {
          ((result.*(std::remove_reference_t<decltype(fs)>::member_ptr) =
                fs.generator.generate()),
           ...);
        },
        gen_tuple);
    return result;
  });
}

}  // namespace st

// =============================================================================
// Embedded Mode Implementation
// =============================================================================

namespace detail {

// Thread-local mode state setters (used by embedded mode)
void set_mode(Mode m);
void set_is_last_run(bool v);
void set_embedded_connection(int fd);
void clear_embedded_connection();
std::string read_line(int fd);
void write_line(int fd, const std::string& line);

}  // namespace detail

/// Run property-based tests using Hegel in embedded mode.
///
/// This function:
/// 1. Creates a Unix socket server
/// 2. Spawns the hegel CLI as a subprocess
/// 3. Accepts connections from hegel (one per test case)
/// 4. Runs the test function for each test case
/// 5. Reports results back to hegel
/// 6. Throws std::runtime_error if any test case fails
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
  std::vector<std::string> args = {hegel_path, "--client-mode", socket_path,
                                   "--no-tui"};
  if (options.test_cases) {
    args.push_back("--test-cases");
    args.push_back(std::to_string(*options.test_cases));
  }
  if (options.debug) {
    args.push_back("--debug");
  }

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
          detail::set_mode(Mode::Embedded);
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

          detail::set_mode(Mode::Standalone);
        } catch (...) {
          detail::clear_embedded_connection();
          detail::set_mode(Mode::Standalone);
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

#endif  // HEGEL_HPP
