#include <hegel/internal.h>
#include <impl.h>
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
            impl::stop_test();
        }
    }
}