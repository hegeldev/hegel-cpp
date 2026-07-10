#include <hegel/internal.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <test_case.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

namespace hegel {

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
        int64_t collection = internal::new_collection(
            *this, static_cast<uint64_t>(min_size), internal::no_max_size);
        uint64_t iteration = 0;
        while (internal::collection_more(*this, collection)) {
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
        if (data_->should_log()) {
            std::cerr << message << std::endl;
        }
    }

} // namespace hegel
