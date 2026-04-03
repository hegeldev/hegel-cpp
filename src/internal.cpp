#include <data.h>
#include <hegel/internal.h>

#include "json_impl.h"

#include <iostream>
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
        if (data->is_last_run) {
            std::cerr << message << std::endl;
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

    bool has_explicit_value(impl::data::TestCaseData* data) {
        return data->explicit_values != nullptr &&
               !data->explicit_values->empty();
    }

    std::any pop_explicit_value(impl::data::TestCaseData* data) {
        std::any val = std::move(data->explicit_values->back());
        data->explicit_values->pop_back();
        return val;
    }

    bool is_explicit_example(impl::data::TestCaseData* data) {
        return data->explicit_values != nullptr;
    }
} // namespace hegel::internal
