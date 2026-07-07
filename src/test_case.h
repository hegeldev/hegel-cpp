#pragma once

/**
 * @cond INTERNAL
 */

#include <hegel.h>
#include <hegel/settings.h>

namespace hegel::impl::test_case {

    // Per-iteration runtime state. `tc` is a borrowed libhegel handle owned
    // by the run loop (src/hegel.cpp); generators reach it through
    // TestCase::data() to drive the typed draw primitives. (The
    // error-reporting context is per-thread: impl::thread_context().)
    struct TestCaseData {
        hegel_test_case_t* tc;
        bool is_final;
        Verbosity verbosity;

        // Whether per-case diagnostics (notes, drawn values) should print:
        // never under Quiet, only on the final replay under Normal, and on
        // every case under Verbose / Debug.
        bool should_log() const {
            switch (verbosity) {
            case Verbosity::Quiet:
                return false;
            case Verbosity::Normal:
                return is_final;
            case Verbosity::Verbose:
            case Verbosity::Debug:
                return true;
            }
            return false; // GCOVR_EXCL_LINE - switch above is exhaustive
        }
    };

} // namespace hegel::impl::test_case

/// @endcond
