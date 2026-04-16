#include <test_case.h>

namespace hegel::impl::test_case {

    static thread_local TestCaseData* current_ = nullptr;

    void set(TestCaseData* data) { current_ = data; }
    void clear() { current_ = nullptr; }
    TestCaseData* get() { return current_; }

} // namespace hegel::impl::test_case
