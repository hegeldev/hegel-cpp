/*
 * hegel.cpp - The hegel::test() entry point.
 *
 * Drives Hegel's native engine (libhegel) in-process through its C ABI:
 * start a run, pull test cases, run the user body, mark each complete, then
 * inspect the aggregate result and replay any counterexamples.
 */

#include <hegel/hegel.h>
#include <hegel/internal.h>
#include <hegel/settings.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <test_case.h>

#include <hegel.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cxxabi.h>
#include <exception>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace hegel {

    namespace {
        constexpr const char* flaky_diagnostic =
            "Flaky test detected: Your test produced different outcomes when "
            "run with the same generated data — it failed when it previously "
            "succeeded, or succeeded when it previously failed. This usually "
            "means your test depends on external state such as global "
            "variables, system time, or external random number generators.";

        // RAII guards for the libhegel handles. Each `*_free` is a no-op on
        // NULL and never throws. (The error-reporting context is not guarded
        // here: impl::thread_context() owns one per thread.)
        struct SettingsGuard {
            hegel_context_t* ctx;
            hegel_settings_t* s = nullptr;
            ~SettingsGuard() { hegel_settings_free(ctx, s); }
        };

        struct RunGuard {
            hegel_context_t* ctx;
            hegel_run_t* run = nullptr;
            ~RunGuard() { hegel_run_free(ctx, run); }
        };

        // Every test-case handle is caller-owned, whatever produced it.
        struct TestCaseGuard {
            hegel_context_t* ctx;
            hegel_test_case_t* tc = nullptr;
            ~TestCaseGuard() { hegel_test_case_free(ctx, tc); }
        };

        // Caller-owned snapshot of a finished run's result.
        struct ResultGuard {
            hegel_context_t* ctx;
            hegel_run_result_t* result = nullptr;
            ~ResultGuard() { hegel_run_result_free(ctx, result); }
        };

        // Caller-owned snapshot of one distinct failure.
        struct FailureGuard {
            hegel_context_t* ctx;
            hegel_failure_t* failure = nullptr;
            ~FailureGuard() { hegel_failure_free(ctx, failure); }
        };

        struct BodyOutcome {
            hegel_status_t status;
            std::string origin;
            std::string message;
            std::exception_ptr exception;
        };

        // Demangle a typeid name, owning the malloc'd result. The fallback
        // covers unparseable input and the demangler's allocation failure.
        std::string demangle(const char* name) {
            int status = 0;
            char* demangled =
                abi::__cxa_demangle(name, nullptr, nullptr, &status);
            if (demangled == nullptr) {
                return name; // GCOVR_EXCL_LINE
            }
            std::string out = demangled;
            std::free(demangled);
            return out;
        }

        // Run the user's test body once and classify the outcome into the
        // libhegel status the caller passes to hegel_mark_complete. The
        // origin is demangled here, before it reaches the engine, so the
        // engine's failure origins are readable as reported.
        BodyOutcome run_body(const std::function<void(TestCase&)>& test_fn,
                             TestCase& tc) {
            try {
                test_fn(tc);
                return {HEGEL_STATUS_VALID, "", ""};
            } catch (const internal::HegelStopTest&) {
                return {HEGEL_STATUS_OVERRUN, "", ""};
            } catch (const internal::HegelReject&) {
                return {HEGEL_STATUS_INVALID, "", ""};
            } catch (const std::exception& e) {
                return {HEGEL_STATUS_INTERESTING, demangle(typeid(e).name()),
                        e.what(), std::current_exception()};
            } catch (...) {
                // Only user code runs inside the try, so anything caught
                // here is a test failure — including a foreign (non-C++)
                // exception, for which the ABI can supply neither a
                // type_info nor an exception_ptr. Substitute a described
                // exception so the re-raise path stays valid.
                std::string origin = "unknown_exception";
                if (const std::type_info* tinfo =
                        abi::__cxa_current_exception_type()) {
                    origin = demangle(tinfo->name());
                }
                std::exception_ptr exception = std::current_exception();
                if (exception == nullptr) {
                    // GCOVR_EXCL_START
                    exception = std::make_exception_ptr(std::runtime_error(
                        "test body raised a foreign (non-C++) exception"));
                    // GCOVR_EXCL_STOP
                }
                return {HEGEL_STATUS_INTERESTING, origin, "", exception};
            }
        }

        void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                           const BodyOutcome& outcome) {
            const char* origin =
                outcome.origin.empty() ? nullptr : outcome.origin.c_str();
            impl::mark_complete(ctx, tc, outcome.status, origin);
        }

        BodyOutcome replay_failure(hegel_context_t* ctx, hegel_settings_t* s,
                                   const char* blob, Verbosity verbosity,
                                   const std::function<void(TestCase&)>& fn) {
            TestCaseGuard tc_guard{ctx};
            tc_guard.tc = impl::test_case_from_blob(ctx, s, blob);
            // GCOVR_EXCL_START
            if (tc_guard.tc == nullptr) {
                // should be handled by check_rc, possible bug in Hegel if hit
                throw std::runtime_error(
                    "Unreachable: failure blob produced a null test case.");
            }
            // GCOVR_EXCL_STOP
            // Positional init (fields: tc, is_final, verbosity) so this
            // TU stays clean under a C++17 (HEGEL_REFLECTION=OFF) build.
            impl::test_case::TestCaseData data{tc_guard.tc,
                                               /*is_final=*/true, verbosity};
            TestCase tc_obj(&data);
            BodyOutcome outcome = run_body(fn, tc_obj);
            mark_complete(ctx, tc_guard.tc, outcome);
            return outcome;
        }

        // Translate hegel::Settings onto a fresh hegel_settings_t handle.
        void apply_settings(hegel_context_t* ctx, hegel_settings_t* s,
                            const Settings& settings) {
            impl::settings_set_test_cases(ctx, s,
                                          settings.test_cases.value_or(100));

            hegel_verbosity_t v = HEGEL_VERBOSITY_NORMAL;
            switch (settings.verbosity) {
            case Verbosity::Quiet:
                v = HEGEL_VERBOSITY_QUIET;
                break;
            case Verbosity::Normal:
                v = HEGEL_VERBOSITY_NORMAL;
                break;
            case Verbosity::Verbose:
                v = HEGEL_VERBOSITY_VERBOSE;
                break;
            case Verbosity::Debug:
                v = HEGEL_VERBOSITY_DEBUG;
                break;
            }
            impl::settings_set_verbosity(ctx, s, v);

            impl::settings_set_seed(ctx, s, settings.seed.value_or(0),
                                    settings.seed.has_value());
            impl::settings_set_derandomize(ctx, s, settings.derandomize);
            impl::settings_set_report_multiple_failures(
                ctx, s, settings.report_multiple_failures);

            switch (settings.database.kind()) {
            case Database::Kind::Unset:
                break;
            case Database::Kind::Disabled:
                impl::settings_set_database(ctx, s, "");
                break;
            case Database::Kind::Path:
                impl::settings_set_database(ctx, s,
                                            settings.database.path().c_str());
                break;
            }

            if (settings.database_key.has_value()) {
                impl::settings_set_database_key(ctx, s,
                                                settings.database_key->c_str());
            }

            uint32_t suppress = 0;
            for (HealthCheck c : settings.suppress_health_check) {
                switch (c) {
                case HealthCheck::FilterTooMuch:
                    suppress |= HEGEL_HC_FILTER_TOO_MUCH;
                    break;
                case HealthCheck::TooSlow:
                    suppress |= HEGEL_HC_TOO_SLOW;
                    break;
                case HealthCheck::TestCasesTooLarge:
                    suppress |= HEGEL_HC_TEST_CASES_TOO_LARGE;
                    break;
                case HealthCheck::LargeInitialTestCase:
                    suppress |= HEGEL_HC_LARGE_INITIAL_TEST_CASE;
                    break;
                }
            }
            if (suppress != 0) {
                impl::settings_set_suppress_health_check(ctx, s, suppress);
            }

            uint32_t phases = 0;
            for (Phase p : settings.phases) {
                switch (p) {
                case Phase::Explicit:
                    phases |= HEGEL_PHASE_EXPLICIT;
                    break;
                case Phase::Reuse:
                    phases |= HEGEL_PHASE_REUSE;
                    break;
                case Phase::Generate:
                    phases |= HEGEL_PHASE_GENERATE;
                    break;
                case Phase::Target:
                    phases |= HEGEL_PHASE_TARGET;
                    break;
                case Phase::Shrink:
                    phases |= HEGEL_PHASE_SHRINK;
                    break;
                }
            }
            impl::settings_set_phases(ctx, s, phases);

            hegel_mode_t mode = HEGEL_MODE_TEST_RUN;
            switch (settings.mode) {
            case Mode::TestRun:
                mode = HEGEL_MODE_TEST_RUN;
                break;
            case Mode::SingleTestCase:
                mode = HEGEL_MODE_SINGLE_TEST_CASE;
                break;
            }
            impl::settings_set_mode(ctx, s, mode);

            hegel_backend_t backend = HEGEL_BACKEND_AUTO;
            switch (settings.backend) {
            case Backend::Auto:
                backend = HEGEL_BACKEND_AUTO;
                break;
            case Backend::Default:
                backend = HEGEL_BACKEND_DEFAULT;
                break;
            case Backend::Urandom:
                backend = HEGEL_BACKEND_URANDOM;
                break;
            }
            impl::settings_set_backend(ctx, s, backend);
        }

        BodyOutcome
        handle_failure(const std::function<void(TestCase&)>& test_fn,
                       hegel_context_t* ctx, hegel_settings_t* s,
                       const Settings& settings,
                       const hegel_failure_t* failure) {
            const char* blob = impl::failure_reproduction_blob(ctx, failure);
            if (blob == nullptr) {
                // GCOVR_EXCL_START
                throw std::runtime_error(
                    "internal error: failure has no reproduction blob");
                // GCOVR_EXCL_STOP
            }
            if (settings.verbosity != Verbosity::Quiet && settings.print_blob) {
                std::fprintf(stderr, "Failure blob: %s\n", blob);
            }
            BodyOutcome outcome =
                replay_failure(ctx, s, blob, settings.verbosity, test_fn);
            if (outcome.status != HEGEL_STATUS_INTERESTING) {
                // GCOVR_EXCL_START
                throw std::runtime_error(flaky_diagnostic);
                // GCOVR_EXCL_STOP
            }
            return outcome;
        }

        void run_from_blob(const std::function<void(TestCase&)>& test_fn,
                           hegel_context_t* ctx, hegel_settings_t* s,
                           const Settings& settings,
                           const std::vector<std::string>& failure_blobs) {
            // multiple blobs are accepted for bookkeeping, but only the first
            // one is run like in the other Hegel libraries
            BodyOutcome outcome =
                replay_failure(ctx, s, failure_blobs.front().c_str(),
                               settings.verbosity, test_fn);

            if (outcome.exception == nullptr) {
                throw std::runtime_error(
                    "The failure blob did not reproduce an error");
            }
            if (settings.verbosity != Verbosity::Quiet) {
                std::fprintf(stderr, "Failure blob %s reproduced an error\n",
                             failure_blobs.front().c_str());
            }

            std::rethrow_exception(outcome.exception);
        }

        void run_from_engine(const std::function<void(TestCase&)>& test_fn,
                             hegel_context_t* ctx, hegel_settings_t* s,
                             const Settings& settings) {
            RunGuard run_guard{ctx};
            run_guard.run = impl::run_start(ctx, s);
            hegel_run_t* run = run_guard.run;

            // Generation loop: pull cases until the engine reports completion
            // (NULL test case), running, marking, and releasing each.
            while (true) {
                TestCaseGuard tc_guard{ctx};
                tc_guard.tc = impl::next_test_case(ctx, run);
                if (tc_guard.tc == nullptr) {
                    break;
                }
                impl::test_case::TestCaseData data{tc_guard.tc,
                                                   /*is_final=*/false,
                                                   settings.verbosity};
                TestCase tc_obj(&data);
                BodyOutcome outcome = run_body(test_fn, tc_obj);
                mark_complete(ctx, tc_guard.tc, outcome);
            }

            ResultGuard result_guard{ctx};
            result_guard.result = impl::run_result(ctx, run);
            hegel_run_result_t* result = result_guard.result;
            hegel_run_status_t run_status =
                impl::run_result_status(ctx, result);

            if (run_status == HEGEL_RUN_STATUS_PASSED) {
                return;
            }

            if (run_status == HEGEL_RUN_STATUS_ERROR) {
                // The run itself failed (health check, nondeterminism, engine
                // panic) and produced no verdict on the property.
                const char* run_err = impl::run_result_error(ctx, result);
                throw std::runtime_error(std::string("Hegel run error: ") +
                                         (run_err ? run_err : "unknown error"));
            }
            // Failed: replay each distinct counterexample as its own block — a
            // "Failure N:" header, then its notes, then its exception.
            size_t failure_count = impl::run_result_failure_count(ctx, result);
            bool quiet = settings.verbosity == Verbosity::Quiet;

            if (failure_count == 1) {
                FailureGuard failure_guard{ctx};
                failure_guard.failure =
                    impl::run_result_failure(ctx, result, 0);
                std::rethrow_exception(handle_failure(test_fn, ctx, s, settings,
                                                      failure_guard.failure)
                                           .exception);
            }

            for (size_t i = 0; i < failure_count; i++) {
                FailureGuard failure_guard{ctx};
                failure_guard.failure =
                    impl::run_result_failure(ctx, result, i);
                if (!quiet) {
                    std::fprintf(stderr, "Failure %zu:\n", i + 1);
                }
                BodyOutcome outcome = handle_failure(test_fn, ctx, s, settings,
                                                     failure_guard.failure);
                if (!quiet && !outcome.message.empty()) {
                    std::fprintf(
                        stderr, "Exception %s: %s\n",
                        impl::failure_origin(ctx, failure_guard.failure),
                        outcome.message.c_str());
                }
            }
            throw std::runtime_error("\nHegel test failed with " +
                                     std::to_string(failure_count) +
                                     " distinct failures");
        }
    } // namespace

    namespace internal {
        namespace {
            struct RegisteredTest {
                const char* name;
                void (*run)();
            };

            // Function-local static so registrations from other translation
            // units' static initializers always find a constructed registry.
            std::vector<RegisteredTest>& test_registry() {
                static std::vector<RegisteredTest> registry;
                return registry;
            }

            std::map<std::string, std::vector<std::string>>& blob_registry() {
                static std::map<std::string, std::vector<std::string>>
                    registry;
                return registry;
            }
        } // namespace

        bool register_test(const char* name, void (*run)()) {
            test_registry().push_back({name, run});
            return true;
        }

        bool register_blob(const char* name, std::vector<const char*> blobs) {
            auto [it, inserted] = blob_registry().insert(
                {name, std::vector<std::string>(blobs.begin(), blobs.end())});
            if (!inserted) {
                // Registration runs during static initialization, so this
                // throw terminates the program — loudly, with this message —
                // rather than silently dropping the new blobs.
                throw std::logic_error(
                    std::string("duplicate HEGEL_REPRODUCE_FAILURE "
                                "registration for test '") +
                    name +
                    "': each test may have at most one "
                    "HEGEL_REPRODUCE_FAILURE annotation. To track several "
                    "blobs, list them all in that one annotation (only the "
                    "first is replayed).");
            }
            return true;
        }

        std::vector<std::string> reproduce_blobs_for(const char* name) {
            auto blob = blob_registry().find(name);
            if (blob == blob_registry().end()) {
                return {};
            }
            return blob->second;
        }
    } // namespace internal

    int run_all_tests() {
        const auto& registry = internal::test_registry();
        size_t failed = 0;
        for (const internal::RegisteredTest& test : registry) {
            try {
                test.run();
            } catch (const std::exception& e) {
                failed++;
                std::fprintf(stderr, "[ FAILED ] %s\n%s\n", test.name,
                             e.what());
            } catch (...) {
                // hegel::test rethrows the failing body's own exception,
                // which need not derive from std::exception. Count it as a
                // failure like any other instead of letting it escape and
                // terminate the process.
                failed++;
                std::string origin = "unknown exception type";
                if (const std::type_info* tinfo =
                        abi::__cxa_current_exception_type()) {
                    origin = demangle(tinfo->name());
                }
                std::fprintf(stderr,
                             "[ FAILED ] %s\ntest threw a non-standard "
                             "exception of type %s\n",
                             test.name, origin.c_str());
            }
        }
        std::fprintf(stderr, "Ran %zu Hegel tests: %zu failed\n",
                     registry.size(), failed);
        return failed == 0 ? 0 : 1;
    }

    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings,
              const std::vector<std::string>& failure_blobs) {
        hegel_context_t* ctx = impl::thread_context();

        SettingsGuard settings_guard{ctx};
        settings_guard.s = impl::settings_new(ctx);
        hegel_settings_t* s = settings_guard.s;
        apply_settings(ctx, s, settings);

        if (failure_blobs.empty()) {
            run_from_engine(test_fn, ctx, s, settings);
        } else {
            run_from_blob(test_fn, ctx, s, settings, failure_blobs);
        }
    }
} // namespace hegel
