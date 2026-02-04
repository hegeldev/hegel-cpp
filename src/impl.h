#pragma once

#include <stdexcept>

// Miscellaneous implementation functions
namespace hegel::impl {
    /* Exception thrown when a test case is rejected
     * and should be discarded rather than counted as a failure.
     */
    class HegelReject : public std::exception {
    public:
        const char* what() const noexcept override { return "assume failed"; }
    };

    /**
    * @brief Stop the current test case immediately.
    *
    * Throws HegelReject which is caught by hegel().
    *
    * @note This function never returns.
    */
    [[noreturn]] void stop_test();
}
