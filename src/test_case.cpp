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

    bool TestCase::has_explicit_value() const {
        return data_->explicit_values != nullptr &&
               !data_->explicit_values->empty();
    }

    std::any TestCase::pop_explicit_value() const {
        std::any val = std::move(data_->explicit_values->back());
        data_->explicit_values->pop_back();
        return val;
    }

    bool TestCase::is_explicit_example() const {
        return data_->explicit_values != nullptr;
    }
} // namespace hegel
