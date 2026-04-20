#pragma once

#include <cstdint>
#include <hegel/settings.h>

namespace hegel::impl {
    class Connection;
}

namespace hegel::impl::test_case {

    struct TestCaseData {
        Connection* connection;
        uint32_t data_stream;
        bool is_last_run;
        Verbosity verbosity;
    };

} // namespace hegel::impl::test_case
