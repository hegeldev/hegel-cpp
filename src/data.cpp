#include <data.h>

// =============================================================================
// Per-test-case State
// =============================================================================
namespace hegel::impl::data {

    static thread_local TestCaseData* current_ = nullptr;

    void set(TestCaseData* data) { current_ = data; }
    void clear() { current_ = nullptr; }
    TestCaseData* get() { return current_; }

} // namespace hegel::impl::data
