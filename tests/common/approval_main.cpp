// Shared main for test binaries that contain approval (snapshot) tests.
// Registers the ApprovalTests GoogleTest listener so snapshot files are
// named after each test.
//
// Snapshots live in tests/approvals/ as *.approved.txt. A mismatch writes
// the actual output to *.received.txt and prints both paths. To accept new
// output after review, run the test binary with
// APPROVAL_TESTS_USE_REPORTER=AutoApproveReporter.
#define APPROVALS_GOOGLETEST
#include <ApprovalTests.hpp>

namespace {
    const auto directory_disposer =
        ApprovalTests::Approvals::useApprovalsSubdirectory("approvals");
    // Report mismatches as text instead of launching a diff tool; override
    // with the APPROVAL_TESTS_USE_REPORTER environment variable.
    const auto reporter_disposer =
        ApprovalTests::Approvals::useAsDefaultReporter(
            std::make_shared<ApprovalTests::QuietReporter>());
} // namespace
