#include <hegel/internal.h>
#include <iostream>
#include <run_state.h>
#include <socket.h>

// Note: the socket part of this namespace is implemented in socket.cpp

namespace hegel::internal {
    void note(const std::string& message) {
        if (!impl::socket::get_embedded_connection()) {
            throw std::runtime_error(
                "note() cannot be called outside of a Hegel test");
        }
        if (impl::run_state::is_last_run()) {
            std::cerr << message << std::endl;
        }
    }

    void assume(bool condition) {
        if (!impl::socket::get_embedded_connection()) {
            throw std::runtime_error(
                "assume() cannot be called outside of a Hegel test");
        }
        if (!condition) {
            stop_test();
        }
    }

    [[noreturn]] void stop_test() { throw HegelReject(); }
} // namespace hegel::internal