#include <hegel/internal.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <test_case.h>

#include <hegel.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
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
        // A blank line opens the framed report's body.
        if (data_->in_report && !*data_->printed_output) {
            std::cerr << "\n";
        }
        *data_->printed_output = true;
        // Every line of a multi-line message keeps the body's indent.
        std::string indent = data_->indent_prefix();
        std::string_view rest = message;
        while (true) {
            size_t end = rest.find('\n');
            if (end == std::string_view::npos) {
                std::cerr << indent << rest << std::endl;
                return;
            }
            std::cerr << indent << rest.substr(0, end) << "\n";
            rest.remove_prefix(end + 1);
        }
    }

    namespace internal {

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
