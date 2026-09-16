#include <hegel/internal.h>
#include <hegel/settings.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <test_case.h>

#include <hegel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <ratio>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace hegel::impl::test_case {

    TestCaseData::~TestCaseData() {
        if (tc != nullptr) {
            hegel_test_case_free(thread_context(), tc);
        }
    }

} // namespace hegel::impl::test_case

namespace hegel {

    TestCase::TestCase(std::unique_ptr<impl::test_case::TestCaseData> data)
        : data_(std::move(data)) {}

    TestCase::TestCase(TestCase&&) noexcept = default;
    TestCase& TestCase::operator=(TestCase&&) noexcept = default;
    TestCase::~TestCase() = default;

    TestCase TestCase::clone() const {
        hegel_test_case_t* handle =
            impl::test_case_clone(impl::thread_context(), data_->tc);
        auto cloned = std::unique_ptr<impl::test_case::TestCaseData>(
            new impl::test_case::TestCaseData{
                handle, data_->is_final, data_->verbosity, data_->note_indent,
                data_->in_report, data_->printed_output});
        cloned->output_mutex = data_->output_mutex;
        cloned->buffer_output = data_->buffer_output;
        cloned->output_lines = data_->output_lines;
        cloned->stateful_step_count = data_->stateful_step_count;
        return TestCase(std::move(cloned));
    }

    void TestCase::assume(bool condition) const {
        if (!condition) {
            throw internal::HegelReject();
        }
    }

    void TestCase::reject() const { throw internal::HegelReject(); }

    void TestCase::target(double score, std::string_view label) const {
        impl::target(*this, score, std::string(label).c_str());
    }

    void TestCase::repeat(const std::function<void()>& body) const {
        // Seed the loop's minimum length, then let the engine decide each
        // iteration: the count is drawn like any collection, so it shrinks.
        int64_t min_size = internal::draw_integer(*this, 0, int64_t{1} << 20);
        internal::CollectionHandle collection(
            *this, static_cast<uint64_t>(min_size), internal::no_max_size);
        uint64_t iteration = 0;
        while (collection.more(*this)) {
            note("// Repetition #" + std::to_string(++iteration));
            try {
                body();
            } catch (const internal::HegelReject&) {
                // Iteration rejected; discard it and keep looping.
                continue;
            }
        }
    }

    void TestCase::note(std::string_view message) const {
        if (!data_->should_log()) {
            return;
        }
        std::lock_guard<std::mutex> lock(*data_->output_mutex);
        bool emit_live = !data_->buffer_output ||
                         data_->verbosity == Verbosity::Verbose ||
                         data_->verbosity == Verbosity::Debug;
        if (data_->in_report && !*data_->printed_output && emit_live) {
            std::cerr << "\n";
        }
        *data_->printed_output = true;
        // Every line of a multi-line message keeps the body's indent.
        std::string indent = data_->indent_prefix();
        std::string prefix;
        if (data_->worker_index.has_value()) {
            std::size_t worker_index = data_->worker_index.value_or(0);
            double elapsed =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - data_->worker_started)
                    .count();
            std::ostringstream out;
            out << "[worker " << worker_index << " +" << std::fixed
                << std::setprecision(3) << elapsed << "ms] ";
            prefix = out.str();
        }
        std::string_view rest = message;
        while (true) {
            size_t end = rest.find('\n');
            std::string line =
                prefix + indent + std::string(rest.substr(0, end));
            if (data_->buffer_output) {
                data_->output_lines->push_back({data_->worker_index, line});
            }
            if (emit_live) {
                std::cerr << line;
            }
            if (end == std::string_view::npos) {
                if (emit_live) {
                    std::cerr << std::endl;
                }
                return;
            }
            if (emit_live) {
                std::cerr << "\n";
            }
            rest.remove_prefix(end + 1);
        }
    }

    namespace internal {

        void set_concurrent_worker(TestCase& tc, std::size_t worker_index) {
            tc.data()->worker_index = worker_index;
            tc.data()->worker_started = std::chrono::steady_clock::now();
        }

        NoteIndentScope::NoteIndentScope(const TestCase& tc) : tc_(tc) {
            tc_.data()->note_indent++;
        }

        NoteIndentScope::~NoteIndentScope() { tc_.data()->note_indent--; }

        DrawLogScope::DrawLogScope(const TestCase& tc, std::string_view name,
                                   bool repeatable)
            : tc_(tc) {
            auto* data = tc_.data();
            outermost_ = data->draw_depth == 0;
            if (outermost_) {
                std::string base(name.empty() ? "draw" : name);
                bool base_repeatable = name.empty() || repeatable;

                int count = ++data->draw_name_counts[base];
                if (!base_repeatable) {
                    // A bare name prints bare on every use; the surrounding
                    // output (e.g. a stateful step header) disambiguates.
                    display_ = base;
                    data->taken_display_names.insert(display_);
                } else {
                    int candidate = count;
                    while (true) {
                        std::string attempt =
                            base + "_" + std::to_string(candidate);
                        if (data->taken_display_names.insert(attempt).second) {
                            display_ = attempt;
                            break;
                        }
                        candidate++;
                    }
                }
            }
            data->draw_depth++;
        }

        DrawLogScope::~DrawLogScope() { tc_.data()->draw_depth--; }

        bool DrawLogScope::should_log() const {
            return outermost_ && tc_.data()->should_log();
        }

        void DrawLogScope::log(const std::string& rendered) const {
            tc_.note("auto " + display_ + " = " + rendered + ";");
        }

    } // namespace internal

} // namespace hegel
