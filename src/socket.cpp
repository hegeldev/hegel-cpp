#include <connection.h>
#include <hegel/cbor.h>
#include <hegel/internal.h>
#include <hegel/options.h>
#include <iostream>
#include <run_state.h>
#include <socket.h>

// =============================================================================
// Socket Communication
// =============================================================================
namespace hegel::impl::socket {
// =============================================================================
// Thread-local State
// =============================================================================
static thread_local Connection* embedded_conn_ = nullptr;
static thread_local uint32_t embedded_data_channel_ = 0;
static thread_local bool test_aborted_ = false;

// =============================================================================
// Functions
// =============================================================================
void set_embedded_connection(Connection* conn, uint32_t data_channel) {
    embedded_conn_ = conn;
    embedded_data_channel_ = data_channel;
    test_aborted_ = false;
}

void clear_embedded_connection() {
    embedded_conn_ = nullptr;
    embedded_data_channel_ = 0;
}

Connection* get_embedded_connection() { return embedded_conn_; }

uint32_t get_embedded_data_channel() { return embedded_data_channel_; }

bool is_test_aborted() { return test_aborted_; }

void set_test_aborted(bool v) { test_aborted_ = v; }
} // namespace hegel::impl::socket

// =============================================================================
// Constants
// =============================================================================
inline constexpr int ASSERTION_FAILURE_EXIT_CODE = 134;

namespace hegel::internal {
cbor::Value communicate_with_socket(const cbor::Value& schema) {
    using namespace cbor;

    auto* conn = impl::socket::get_embedded_connection();
    uint32_t data_channel = impl::socket::get_embedded_data_channel();

    if (!conn) {
        std::cerr << "hegel: no connection set\n";
        std::exit(ASSERTION_FAILURE_EXIT_CODE);
    }

    // Build generate request as CBOR
    Value request = map({{"command", text("generate")}, {"schema", schema}});

    if (std::getenv("HEGEL_DEBUG")) {
        std::cerr << "REQUEST: " << request.dump() << "\n";
    }

    // Send request and get response
    Value response = conn->request(data_channel, request);

    if (std::getenv("HEGEL_DEBUG")) {
        std::cerr << "RESPONSE: " << response.dump() << "\n";
    }

    // Handle errors
    if (auto error = map_get(response, "error")) {
        auto error_type = as_text(map_get(response, "type"));
        if (error_type == "StopTest" || error_type == "Overflow") {
            impl::socket::set_test_aborted(true);
            internal::stop_test();
        }
        throw std::runtime_error("hegel: " +
                                 as_text(*error).value_or("unknown error"));
    }

    // Auto-log generated value during final replay (counterexample)
    if (impl::run_state::is_last_run()) {
        if (auto result = map_get(response, "result")) {
            std::cerr << "Generated: " << result->dump() << "\n";
        }
    }

    return response;
}

} // namespace hegel::internal
