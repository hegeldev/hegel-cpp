#include <hegel/internal.h>
#include <hegel/test_case.h>
#include <test_case.h>

#include <iostream>
#include <string_view>

namespace hegel {

    void TestCase::assume(bool condition) const {
        if (!condition) {
            throw internal::HegelReject();
        }
    }

    void TestCase::note(std::string_view message) const {
        if (data_->is_last_run) {
            std::cerr << message << std::endl;
        }
    }

} // namespace hegel
