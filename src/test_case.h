#pragma once

/**
 * @cond INTERNAL
 */

#include <hegel.h>
#include <hegel/settings.h>

namespace hegel::impl::test_case {

    // Per-iteration runtime state. `ctx` and `tc` are borrowed libhegel
    // handles owned by the run loop (src/hegel.cpp); generators reach them
    // through TestCase::data() to drive `hegel_generate`.
    struct TestCaseData {
        hegel_context_t* ctx;
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
