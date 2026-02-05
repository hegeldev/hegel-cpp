#include <hegel/internal.h>
#include <run_state.h>
#include <iostream>

// Note: the socket part of this namespace is implemented in socket.cpp

namespace hegel::internal {
    void note(const std::string& message) {
        if (impl::run_state::is_last_run()) {
            std::cerr << message << std::endl;
        }
    }

    void assume(bool condition) {
        if (!condition) {
            stop_test();
        }
    }

    [[noreturn]] void stop_test() { throw HegelReject(); }
}