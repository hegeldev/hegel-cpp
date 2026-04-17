#include <gtest/gtest.h>

#include "common/temp_project.h"
#include "common/utils.h"

#include <string>

using hegel::tests::common::assert_matches_regex;
using hegel::tests::common::SubprocessResult;
using hegel::tests::common::TempCppProject;

constexpr const char* FAILING_TEST_CODE = R"cpp(
#include <cstdint>
#include <hegel/hegel.h>
#include <stdexcept>
#include <string>

namespace gs = hegel::generators;

int main() {
    hegel::hegel(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            throw std::runtime_error("intentional failure: " +
                                     std::to_string(x));
        },
        {.database = hegel::settings::Database::disabled()});
    return 0;
}
)cpp";

TEST(Output, FailingTest) {
    SubprocessResult r = TempCppProject().main_file(FAILING_TEST_CODE).run();
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Generated: 0\b)");
    assert_matches_regex(r.stderr_data, R"(Hegel test failed)");
}

constexpr const char* STABLE_ORIGIN_TEST_CODE = R"cpp(
#include <cstdint>
#include <hegel/hegel.h>
#include <stdexcept>
#include <string>

namespace gs = hegel::generators;

int main() {
    hegel::hegel(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 10) {
                throw std::runtime_error("failure with x=" +
                                         std::to_string(x));
            }
        },
        {.database = hegel::settings::Database::disabled()});
    return 0;
}
)cpp";

TEST(Output, OriginStableAcrossDrawnValues) {
    SubprocessResult r =
        TempCppProject().main_file(STABLE_ORIGIN_TEST_CODE).run();
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Generated: 10\b)");
    assert_matches_regex(r.stderr_data, R"(Hegel test failed)");
}

constexpr const char* THROW_INT_TEST_CODE = R"cpp(
#include <cstdint>
#include <hegel/hegel.h>

namespace gs = hegel::generators;

int main() {
    hegel::hegel(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 5) {
                throw 42;
            }
        },
        {.database = hegel::settings::Database::disabled()});
    return 0;
}
)cpp";

TEST(Output, NonStdExceptionIsHandled) {
    SubprocessResult r = TempCppProject().main_file(THROW_INT_TEST_CODE).run();
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Hegel test failed)");
    assert_matches_regex(r.stderr_data, R"(Generated: 5\b)");
}

constexpr const char* THROW_CUSTOM_TEST_CODE = R"cpp(
#include <cstdint>
#include <hegel/hegel.h>

struct MyError {};

namespace gs = hegel::generators;

int main() {
    hegel::hegel(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 5) {
                throw MyError{};
            }
        },
        {.database = hegel::settings::Database::disabled()});
    return 0;
}
)cpp";

TEST(Output, CustomNonStdExceptionIsHandled) {
    SubprocessResult r =
        TempCppProject().main_file(THROW_CUSTOM_TEST_CODE).run();
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data, R"(Hegel test failed)");
    assert_matches_regex(r.stderr_data, R"(Generated: 5\b)");
}

constexpr const char* EXCEPTION_MESSAGE_TEST_CODE = R"cpp(
#include <cstdint>
#include <hegel/hegel.h>
#include <stdexcept>
#include <string>

namespace gs = hegel::generators;

int main() {
    hegel::hegel(
        [](hegel::TestCase& tc) {
            int32_t x = tc.draw(gs::integers<int32_t>());
            if (x >= 7) {
                throw std::runtime_error("custom exception for x=" +
                                         std::to_string(x));
            }
        },
        {.database = hegel::settings::Database::disabled()});
    return 0;
}
)cpp";

TEST(Output, ExceptionMessageIsShown) {
    SubprocessResult r =
        TempCppProject().main_file(EXCEPTION_MESSAGE_TEST_CODE).run();
    EXPECT_NE(r.exit_code, 0);
    assert_matches_regex(r.stderr_data,
                         R"(Hegel test failed: custom exception for x=7)");
}
