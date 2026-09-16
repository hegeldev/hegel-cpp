#pragma once

/**
 * @cond INTERNAL
 */

#include <hegel.h>
#include <hegel/settings.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace hegel::impl::test_case {

    struct WorkerOutputLine {
        std::optional<std::size_t> worker;
        std::string text;
    };

    // Per-case runtime state.
    // Generators reach `tc` through TestCase::data() to drive the typed draw
    // primitives. (The error-reporting context is per-thread.
    struct TestCaseData {
        hegel_test_case_t* tc;
        bool is_final;
        Verbosity verbosity;
        int note_indent = 0;
        // True while this case's output forms the body of a framed failure
        // report: the body opens with a blank line and sits at note_indent.
        bool in_report = false;
        // Whether any note or drawn value has printed. Shared with clones,
        // which print into the same body.
        std::shared_ptr<bool> printed_output = std::make_shared<bool>(false);
        std::shared_ptr<std::mutex> output_mutex =
            std::make_shared<std::mutex>();
        std::optional<std::size_t> worker_index;
        std::chrono::steady_clock::time_point worker_started;
        bool buffer_output = false;
        std::shared_ptr<std::vector<WorkerOutputLine>> output_lines =
            std::make_shared<std::vector<WorkerOutputLine>>();
        int draw_depth = 0;
        std::map<std::string, int> draw_name_counts;
        std::set<std::string> taken_display_names;
        // Settings::stateful_step_count for this run, read when the case
        // creates a state machine.
        int64_t stateful_step_count = 50;

        // Releases the owned libhegel handle. The owning TestCase's
        // unique_ptr runs this on scope exit.
        ~TestCaseData();

        // leading whitespace for a printed note/drawn value at the current
        // nesting depth.
        std::string indent_prefix() const {
            return std::string(static_cast<std::size_t>(note_indent) * 2, ' ');
        }

        // Whether per-case diagnostics (notes, drawn values) should print:
        // never under Quiet, only on the final replay under Normal, and on
        // every case under Verbose / Debug.
        bool should_log() const {
            switch (verbosity) {
            case Verbosity::Quiet:
                return false;
            case Verbosity::Normal:
                return is_final || buffer_output;
            case Verbosity::Verbose:
            case Verbosity::Debug:
                return true;
            }
            return false; // GCOVR_EXCL_LINE - switch above is exhaustive
        }
    };

} // namespace hegel::impl::test_case

/// @endcond
