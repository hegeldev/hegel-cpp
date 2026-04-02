#pragma once

#include <cstdint>
#include <hegel/json.h>
#include <hegel/options.h>
#include <vector>

namespace hegel::impl {
    class Connection;
}

// =============================================================================
// Per-test-case State
// =============================================================================
namespace hegel::impl::data {

    struct TestCaseData {
        Connection* connection;
        uint32_t data_channel;
        bool is_last_run;
        bool test_aborted;
        options::Verbosity verbosity;
        std::vector<hegel::internal::json::json>* explicit_values = nullptr;
    };

    void set(TestCaseData* data);
    void clear();
    TestCaseData* get();

} // namespace hegel::impl::data
