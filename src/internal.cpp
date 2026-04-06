#include <data.h>
#include <hegel/internal.h>

#include <stdexcept>
#include <string>

// Note: the socket part of this namespace is implemented in socket.cpp

namespace hegel::internal {
    impl::data::TestCaseData* get_test_case_data() { return impl::data::get(); }

    void note(const std::string& message) {
        auto* data = get_test_case_data();
        if (!data) {
            throw std::runtime_error(
                "note() cannot be called outside of a Hegel test");
        }
    }

    void assume(bool condition) {
        if (!get_test_case_data()) {
            throw std::runtime_error(
                "assume() cannot be called outside of a Hegel test");
        }
        if (!condition) {
            stop_test();
        }
    }

    [[noreturn]] void stop_test() { throw HegelReject(); }
} // namespace hegel::internal
