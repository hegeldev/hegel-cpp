#pragma once

/**
 * @file gtest.h
 * @brief GoogleTest integration.
 *
 * Include this header, or include `<gtest/gtest.h>` before `<hegel/hegel.h>`,
 * to run properties inside GoogleTest tests:
 *
 * @code{.cpp}
 * #include <hegel/gtest.h>
 *
 * TEST(Arithmetic, AdditionCommutes) {
 *     hegel::test([](hegel::TestCase& tc) {
 *         auto x = tc.draw("x", hegel::generators::integers<int>());
 *         auto y = tc.draw("y", hegel::generators::integers<int>());
 *         ASSERT_EQ(x + y, y + x);
 *     });
 * }
 * @endcode
 *
 * The integration raises hegel::GTestFailure for the assertions a test case
 * fails and it names the enclosing test, which heads the failure report and
 * scopes the example database, so counterexamples persist and replay per test.
 *
 * The integration is always on where GoogleTest is. Hegel must see a failed
 * assertion to fail the test case that ran it.
 */

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "hegel.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace hegel {

    /**
     * @brief The exception the GoogleTest integration raises for a test case
     * that fails an assertion.
     *
     * The message holds every failed assertion of that case, each prefixed with
     * its source position.
     *
     * Assertions on different lines are different bugs, and failing different
     * combinations of assertions count as different bugs.
     */
    class GTestFailure : public std::runtime_error, public FailureOrigin {
      public:
        /// @param origin The positions of the failed assertions.
        /// @param message The failed assertions of one test case.
        GTestFailure(std::string origin, const std::string& message)
            : std::runtime_error(message), origin_(std::move(origin)) {}

        /// @return The positions of the assertions this case failed.
        std::string failure_origin() const override { return origin_; }

      private:
        std::string origin_;
    };

    /// @cond INTERNAL
    namespace internal {
        namespace gtest_hooks {

            // "Suite.Name" of the GoogleTest test that runs now.
            inline std::string current_test_name() {
                const testing::TestInfo* info =
                    testing::UnitTest::GetInstance()->current_test_info();
                if (info == nullptr) {
                    return {};
                }
                return std::string(info->test_suite_name()) + "." +
                       info->name();
            }

            // Where one failed assertion is written, or "<unknown>" for an
            // assertion GoogleTest could not place.
            inline std::string position(const testing::TestPartResult& part) {
                if (part.file_name() == nullptr) {
                    return "<unknown>";
                }
                return std::string(part.file_name()) + ":" +
                       std::to_string(part.line_number());
            }

            struct Failures {
                std::string message;
                std::string origin;
            };

            inline Failures
            collect_failures(const testing::TestPartResultArray& recorded) {
                Failures out;
                for (int i = 0; i < recorded.size(); i++) {
                    const testing::TestPartResult& part =
                        recorded.GetTestPartResult(i);
                    if (!part.failed()) {
                        continue;
                    }
                    if (!out.message.empty()) {
                        out.message += "\n";
                        out.origin += ", ";
                    }
                    out.message += position(part) + ": " + part.message();
                    out.origin += position(part);
                }
                return out;
            }

            // Runs one test-case body and collects GoogleTest assertions, then
            // raises them as one exception.
            inline void run_case(const std::function<void()>& body) {
                if (testing::UnitTest::GetInstance()->current_test_info() ==
                    nullptr) {
                    body();
                    return;
                }
                testing::TestPartResultArray recorded;
                {
                    testing::ScopedFakeTestPartResultReporter reporter(
                        testing::ScopedFakeTestPartResultReporter::
                            INTERCEPT_ONLY_CURRENT_THREAD,
                        &recorded);
                    body();
                }
                Failures failures = collect_failures(recorded);
                if (!failures.message.empty()) {
                    throw GTestFailure("hegel::GTestFailure at " +
                                           failures.origin,
                                       failures.message);
                }
            }

        } // namespace gtest_hooks

        [[maybe_unused]] static const bool hegel_gtest_hooks_installed =
            install_framework_hooks(
                {&gtest_hooks::current_test_name, &gtest_hooks::run_case});
    } // namespace internal
    /// @endcond

} // namespace hegel
