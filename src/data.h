#pragma once

#include <cstdint>
#include <hegel/options.h>

namespace hegel::impl {
    class Connection;
    class DataSource;
} // namespace hegel::impl

// =============================================================================
// Per-test-case State
// =============================================================================
namespace hegel::impl::data {

    struct TestCaseData {
        DataSource* source;
        bool is_last_run;
        bool test_aborted;
        options::Verbosity verbosity;
    };

    void set(TestCaseData* data);
    void clear();
    TestCaseData* get();

} // namespace hegel::impl::data
