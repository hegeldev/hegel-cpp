#pragma once

#include <string_view>

namespace hegel::impl::test_case {
    struct TestCaseData;
}

namespace hegel::generators {
    template <typename T> class Generator;
}

namespace hegel {

    /**
     * @brief Handle to the currently-executing test case.
     *
     * A @c TestCase is passed as the sole argument to the callback given to
     * hegel::test(). It is the main way a test definition interacts with Hegel.
     *
     * @c TestCase is a non-owning handle into state managed by the Hegel
     * runner. It is neither copyable nor movable, and must not outlive the
     * test-case callback.
     *
     * @code{.cpp}
     * hegel::test([](hegel::TestCase& tc) {
     *     namespace gs = hegel::generators;
     *     auto x = tc.draw(gs::integers<int>({.min_value = 0}));
     *     tc.assume(x != 0);
     *     tc.note("x = " + std::to_string(x));
     * });
     * @endcode
     */
    class TestCase {
      public:
        TestCase(const TestCase&) = delete;
        TestCase& operator=(const TestCase&) = delete;
        TestCase(TestCase&&) = delete;
        TestCase& operator=(TestCase&&) = delete;

        /**
         * @brief Draw a random value from a generator.
         *
         * @tparam T The value type produced by @p gen
         * @param gen The generator to draw from
         * @return A freshly generated value of type T
         */
        template <typename T> T draw(const generators::Generator<T>& gen) const;

        /**
         * @brief Reject the current test case if @p condition is false.
         *
         * @code{.cpp}
         * auto age = tc.draw(gs::integers<int>());
         * tc.assume(age >= 18);
         * @endcode
         *
         * @param condition Value that must be true for the test case to
         *                  continue. If false, the current test case is
         *                  rejected.
         */
        void assume(bool condition) const;

        /**
         * @brief Record a message that will be printed on the final replay
         *        of a failing test case.
         *
         * @param message The message to record.
         */
        void note(std::string_view message) const;

        /// @cond INTERNAL
        explicit TestCase(impl::test_case::TestCaseData* data) : data_(data) {}

        // Generators reach through this accessor to talk to the backend.
        // Not part of the user-facing API.
        impl::test_case::TestCaseData* data() const { return data_; }
        /// @endcond

      private:
        impl::test_case::TestCaseData* data_;
    };

} // namespace hegel
