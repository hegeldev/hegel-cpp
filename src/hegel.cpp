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

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cxxabi.h>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#if defined(__APPLE__) || defined(__ELF__)
#include <dlfcn.h>
#endif
// needed to wrap __cxa_throw
#if defined(RTLD_NEXT)
#define HEGEL_HAS_THROW_SITE 1

namespace {
    // The return address of the innermost throw this thread made. One per
    // thread, since clone() and spawn() run test-case bodies on their own.
    thread_local const void* last_throw_address = nullptr;

    // GCC declares `__cxa_throw` for its own throw statements with `void*` in
    // place of the ABI's `std::type_info*`. The definition below must use the
    // same type, or the two declarations conflict.
#if defined(__clang__)
    using ThrowTypeInfo = std::type_info;
#else
    using ThrowTypeInfo = void;
#endif
} // namespace

// The stack unwinds before run_body() catches, so a backtrace taken there names
// run_body() as the origin for every failure. `__cxa_throw` runs while the
// throwing frame is still live, so Hegel defines its own and forwards to the
// real one.
// NOLINTNEXTLINE(bugprone-reserved-identifier) - the ABI hook name is fixed
extern "C" void __cxa_throw(void* exception, ThrowTypeInfo* type_info,
                            void (*destructor)(void*)) {
    // store for determining origin later
    last_throw_address = __builtin_return_address(0);

    using ThrowFn = void (*)(void*, ThrowTypeInfo*, void (*)(void*));
    // find the real __cxa_throw and cache
    static ThrowFn forward =
        reinterpret_cast<ThrowFn>(dlsym(RTLD_NEXT, "__cxa_throw"));
    // call the real __cxa_throw
    forward(exception, type_info, destructor);
    __builtin_unreachable(); // forward() never returns
}
#endif

namespace hegel {

    namespace {
        /*
        Needed because concurrent execution crosses threads. The main thread
        cannot access the worker’s thread-local origin otherwise.
        */
        thread_local std::optional<std::string> concurrent_exception_origin;

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

        // How a report prints its "rerun with:" line. The annotation form
        // names a test defined by the HEGEL_TEST macro.
        enum class RerunForm { Call, Annotation };

        // The installed test framework integration
        internal::FrameworkHooks& framework_hooks() {
            static internal::FrameworkHooks hooks;
            return hooks;
        }

        // Names the framework test that runs now, or "" outside one.
        std::string framework_test_name() {
            if (framework_hooks().current_test_name == nullptr) {
                return {};
            }
            return framework_hooks().current_test_name();
        }

        // Runs one test-case body, through the framework integration when
        // there is one so it can raise the failures it recorded.
        void run_case(const std::function<void()>& body) {
            if (framework_hooks().run_case == nullptr) {
                body();
                return;
            }
            framework_hooks().run_case(body);
        }

        // Scopes the example database to one test. The name is the framework
        // test that runs now, or the function that called hegel::test()
        Settings with_default_database_key(Settings settings, const char* file,
                                           const std::string& name) {
            if (settings.database_key.has_value() || name.empty() ||
                settings.database.kind() == Database::Kind::Disabled) {
                return settings;
            }
            settings.database_key =
                std::string(file == nullptr ? "" : file) + "::" + name;
            return settings;
        }

        struct BodyOutcome {
            hegel_status_t status;
            std::string origin;
            // Exception type, as the report's "Exception:" line names it.
            std::string type_name;
            std::string message;
            std::exception_ptr exception;
        };

        struct NondeterministicFailure {
            BodyOutcome outcome;
            std::vector<impl::test_case::WorkerOutputLine> lines;
        };

        std::vector<std::string> group_concurrent_output(
            const std::vector<impl::test_case::WorkerOutputLine>& records) {
            std::vector<std::string> lines;
            std::vector<impl::test_case::WorkerOutputLine> round;
            auto append_grouped_worker_lines = [&] {
                std::stable_sort(round.begin(), round.end(),
                                 [](const auto& left, const auto& right) {
                                     return *left.worker < *right.worker;
                                 });
                for (const auto& line : round) {
                    lines.push_back(line.text);
                }
                round.clear();
            };
            for (const auto& record : records) {
                if (record.worker.has_value()) {
                    round.push_back(record);
                } else {
                    append_grouped_worker_lines();
                    lines.push_back(
                        record.text); // text from main thread has null worker
                }
            }
            append_grouped_worker_lines();
            return lines;
        }

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

        std::string to_hex(uintptr_t value) {
            std::array<char, 2 * sizeof(uintptr_t)> buf;
            auto [ptr, ec] =
                std::to_chars(buf.data(), buf.data() + buf.size(), value, 16);
            return std::string(buf.data(), ptr);
        }

        // Return the most recent throw in a thread as "<binary>+0x<offset
        // from its load address>".
        std::string last_throw_site() {
#if defined(HEGEL_HAS_THROW_SITE)
            // cache sites to prevent repeated lookups during shrinking
            static thread_local std::map<const void*, std::string> sites;
            auto known = sites.find(last_throw_address);
            if (known != sites.end()) {
                return known->second;
            }

            std::string site;
            Dl_info info;
            // translate the throw address to global offset in the binary that
            // contains it
            if (dladdr(last_throw_address, &info) != 0 &&
                info.dli_fname != nullptr) {
                auto offset = static_cast<uintptr_t>(
                    static_cast<const char*>(last_throw_address) -
                    static_cast<const char*>(info.dli_fbase));
                site = std::string(info.dli_fname) + "+0x" + to_hex(offset);
            }
            sites.emplace(last_throw_address, site);
            return site;
#else
            return {};
#endif
        }

        std::string origin_of(const FailureOrigin* named,
                              const std::string& type) {
            if (named != nullptr) {
                return named->failure_origin();
            }
            std::string site = last_throw_site();
            return site.empty() ? type : type + " at " + site;
        }

        // Run the user's test body once and classify the outcome into the
        // libhegel status the caller passes to hegel_mark_complete. The
        // origin is demangled here, before it reaches the engine, so the
        // engine's failure origins are readable as reported.
        BodyOutcome run_body(const std::function<void(TestCase&)>& test_fn,
                             TestCase& tc) {
            try {
                run_case([&] { test_fn(tc); });
                return {HEGEL_STATUS_VALID, "", "", ""};
            } catch (const internal::HegelStopTest&) {
                return {HEGEL_STATUS_OVERRUN, "", "", ""};
            } catch (const internal::HegelReject&) {
                return {HEGEL_STATUS_INVALID, "", "", ""};
            } catch (const std::exception& e) {
                std::string type = demangle(typeid(e).name());
                std::string origin;
                if (concurrent_exception_origin.has_value()) {
                    origin = std::move(*concurrent_exception_origin);
                    concurrent_exception_origin.reset();
                } else {
                    origin =
                        origin_of(dynamic_cast<const FailureOrigin*>(&e), type);
                }
                return {HEGEL_STATUS_INTERESTING, origin, type, e.what(),
                        std::current_exception()};
            } catch (...) {
                // Only user code runs inside the try, so anything caught
                // here is a test failure — including a foreign (non-C++)
                // exception, for which the ABI can supply neither a
                // type_info nor an exception_ptr. Substitute a described
                // exception so the re-raise path stays valid.
                std::string type = "unknown_exception";
                if (const std::type_info* tinfo =
                        abi::__cxa_current_exception_type()) {
                    type = demangle(tinfo->name());
                }
                std::string origin;
                if (concurrent_exception_origin.has_value()) {
                    origin = std::move(*concurrent_exception_origin);
                    concurrent_exception_origin.reset();
                } else {
                    origin = origin_of(nullptr, type);
                }
                std::exception_ptr exception = std::current_exception();
                if (exception == nullptr) {
                    // GCOVR_EXCL_START
                    exception = std::make_exception_ptr(std::runtime_error(
                        "test body raised a foreign (non-C++) exception"));
                    // GCOVR_EXCL_STOP
                }
                return {HEGEL_STATUS_INTERESTING, origin, type, "", exception};
            }
        }

        void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                           const BodyOutcome& outcome) {
            const char* origin =
                outcome.origin.empty() ? nullptr : outcome.origin.c_str();
            impl::mark_complete(ctx, tc, outcome.status, origin);
        }

        // Replays one counterexample. `in_report` wraps the replay's output in
        // a framed report's body: it opens with a blank line and is indented.
        BodyOutcome replay_failure(hegel_context_t* ctx, hegel_settings_t* s,
                                   const char* blob, Verbosity verbosity,
                                   bool in_report,
                                   const std::function<void(TestCase&)>& fn,
                                   bool* printed_output = nullptr) {
            hegel_test_case_t* handle = impl::test_case_from_blob(ctx, s, blob);
            // GCOVR_EXCL_START
            if (handle == nullptr) {
                // should be handled by check_rc, possible bug in Hegel if hit
                throw std::runtime_error(
                    "Unreachable: failure blob produced a null test case.");
            }
            // GCOVR_EXCL_STOP
            // Positional init (fields: tc, is_final, verbosity) so this
            // TU stays clean under a C++17 (HEGEL_REFLECTION=OFF) build.
            TestCase tc_obj(std::unique_ptr<impl::test_case::TestCaseData>(
                new impl::test_case::TestCaseData{
                    handle, /*is_final=*/true, verbosity,
                    /*note_indent=*/in_report ? 1 : 0, in_report}));
            BodyOutcome outcome = run_body(fn, tc_obj);
            if (printed_output != nullptr) {
                *printed_output = *tc_obj.data()->printed_output;
            }
            mark_complete(ctx, tc_obj.data()->tc, outcome);
            return outcome;
        }

        // Width the framed report's header rule is padded to.
        constexpr size_t frame_width = 72;

        void print_failure_header(const std::optional<TestLocation>& location,
                                  uint64_t cases_run,
                                  uint64_t cases_discarded) {
            std::string title = "Failure";
            if (location.has_value()) {
                title += ": " + location->name + " (" + location->file + ":" +
                         std::to_string(location->line) + ")";
            }
            std::string prefix = "--- " + title + " ";
            size_t dashes = prefix.size() + 3 >= frame_width
                                ? 3
                                : frame_width - prefix.size();
            std::fprintf(stderr, "%s%s\n", prefix.c_str(),
                         std::string(dashes, '-').c_str());
            std::fprintf(stderr,
                         "Falsified after %llu test case%s (%llu discarded):\n",
                         static_cast<unsigned long long>(cases_run),
                         cases_run == 1 ? "" : "s",
                         static_cast<unsigned long long>(cases_discarded));
        }

        // Closes one failure section: the exception it raised, then the line
        // that replays it.
        void print_failure_body(const Settings& settings,
                                const std::optional<TestLocation>& location,
                                RerunForm rerun_form,
                                const BodyOutcome& outcome, const char* blob,
                                bool printed_output) {
            if (printed_output) {
                std::fprintf(stderr, "\n");
            }
            std::fprintf(stderr, "Exception: %s%s\n", outcome.type_name.c_str(),
                         outcome.message.empty()
                             ? ""
                             : (": " + outcome.message).c_str());
            if (!settings.print_blob) {
                return;
            }
            if (rerun_form == RerunForm::Annotation && location.has_value()) {
                std::fprintf(
                    stderr, "rerun with: HEGEL_REPRODUCE_FAILURE(%s, \"%s\")\n",
                    location->name.c_str(), blob);
            } else {
                std::fprintf(
                    stderr,
                    "rerun with: hegel::test(test_fn, settings, {\"%s\"})\n",
                    blob);
            }
        }

        // Translate hegel::Settings onto a fresh hegel_settings_t handle.
        void apply_settings(hegel_context_t* ctx, hegel_settings_t* s,
                            const Settings& settings) {
            // Mode::SingleTestCase produces exactly one test case with no
            // shrinking. The engine has no dedicated setting for this, so it
            // is built from the ordinary knobs: cap generation at one case
            // and drop the Shrink phase below.
            bool single = settings.mode == Mode::SingleTestCase;
            impl::settings_set_test_cases(
                ctx, s, single ? 1 : settings.test_cases.value_or(100));
            impl::settings_set_stateful_step_count(
                ctx, s, settings.stateful_step_count);

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
            if (single) {
                phases &= ~static_cast<uint32_t>(HEGEL_PHASE_SHRINK);
            }
            impl::settings_set_phases(ctx, s, phases);

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

        // Replays one counterexample as the body of a failure section and
        // closes the section with its exception and rerun line.
        BodyOutcome
        handle_failure(const std::function<void(TestCase&)>& test_fn,
                       hegel_context_t* ctx, hegel_settings_t* s,
                       const Settings& settings,
                       const std::optional<TestLocation>& location,
                       RerunForm rerun_form, const hegel_failure_t* failure) {
            const char* blob = impl::failure_reproduction_blob(ctx, failure);
            if (blob == nullptr) {
                // GCOVR_EXCL_START
                throw std::runtime_error(
                    "internal error: failure has no reproduction blob");
                // GCOVR_EXCL_STOP
            }
            bool quiet = settings.verbosity == Verbosity::Quiet;
            bool printed_output = false;
            BodyOutcome outcome =
                replay_failure(ctx, s, blob, settings.verbosity,
                               /*in_report=*/!quiet, test_fn, &printed_output);
            if (outcome.status != HEGEL_STATUS_INTERESTING) {
                // GCOVR_EXCL_START
                throw std::runtime_error(flaky_diagnostic);
                // GCOVR_EXCL_STOP
            }
            if (!quiet) {
                print_failure_body(settings, location, rerun_form, outcome,
                                   blob, printed_output);
            }
            return outcome;
        }

        void run_from_blob(const std::function<void(TestCase&)>& test_fn,
                           hegel_context_t* ctx, hegel_settings_t* s,
                           const Settings& settings,
                           const std::vector<std::string>& failure_blobs) {
            // multiple blobs are accepted for bookkeeping, but only the first
            // one is run like in the other Hegel libraries
            BodyOutcome outcome = replay_failure(
                ctx, s, failure_blobs.front().c_str(), settings.verbosity,
                /*in_report=*/false, test_fn);

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
                             const Settings& settings,
                             const std::optional<TestLocation>& location,
                             RerunForm rerun_form) {
            RunGuard run_guard{ctx};
            run_guard.run = impl::run_start(ctx, s);
            hegel_run_t* run = run_guard.run;

            uint64_t cases_run = 0;
            uint64_t cases_discarded = 0;
            bool seen_interesting = false;
            std::optional<NondeterministicFailure> nondeterministic_failure;

            // Generation loop: pull cases until the engine reports completion
            // (NULL test case), running, marking, and releasing each.
            while (true) {
                hegel_test_case_t* handle = impl::next_test_case(ctx, run);
                if (handle == nullptr) {
                    break;
                }
                bool nondeterministic =
                    impl::test_case_is_nondeterministic(ctx, handle);
                auto data = std::unique_ptr<impl::test_case::TestCaseData>(
                    new impl::test_case::TestCaseData{
                        handle, /*is_final=*/false, settings.verbosity});
                data->buffer_output = nondeterministic;
                TestCase tc_obj(std::move(data));
                BodyOutcome outcome = run_body(test_fn, tc_obj);
                if (nondeterministic &&
                    outcome.status == HEGEL_STATUS_INTERESTING) {
                    nondeterministic_failure = NondeterministicFailure{
                        outcome, *tc_obj.data()->output_lines};
                }
                if (!seen_interesting) {
                    if (outcome.status == HEGEL_STATUS_INTERESTING) {
                        cases_run++;
                        seen_interesting = true;
                    } else if (outcome.status == HEGEL_STATUS_VALID) {
                        cases_run++;
                    } else if (outcome.status == HEGEL_STATUS_INVALID) {
                        cases_discarded++;
                    }
                }
                mark_complete(ctx, tc_obj.data()->tc, outcome);
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

            bool quiet = settings.verbosity == Verbosity::Quiet;

            if (run_status == HEGEL_RUN_STATUS_FAILED_NONDETERMINISTIC) {
                if (!nondeterministic_failure.has_value()) {
                    // GCOVR_EXCL_START
                    throw std::runtime_error(
                        "internal error: nondeterministic failure was not "
                        "captured");
                    // GCOVR_EXCL_STOP
                }

                if (!quiet) {
                    print_failure_header(location, cases_run, cases_discarded);
                    std::vector<std::string> lines = group_concurrent_output(
                        nondeterministic_failure->lines);
                    for (const std::string& line : lines) {
                        std::fprintf(stderr, "%s\n", line.c_str());
                    }
                    const BodyOutcome& outcome =
                        nondeterministic_failure->outcome;
                    if (!lines.empty()) {
                        std::fprintf(stderr, "\n");
                    }
                    std::fprintf(stderr, "Exception: %s%s\n",
                                 outcome.type_name.c_str(),
                                 outcome.message.empty()
                                     ? ""
                                     : (": " + outcome.message).c_str());
                }
                std::rethrow_exception(
                    nondeterministic_failure->outcome.exception);
            }
            // Failed: one framed report, holding one section per distinct
            // counterexample.
            size_t failure_count = impl::run_result_failure_count(ctx, result);
            if (!quiet) {
                print_failure_header(location, cases_run, cases_discarded);
            }

            if (failure_count == 1) {
                FailureGuard failure_guard{ctx};
                failure_guard.failure =
                    impl::run_result_failure(ctx, result, 0);
                std::rethrow_exception(handle_failure(test_fn, ctx, s, settings,
                                                      location, rerun_form,
                                                      failure_guard.failure)
                                           .exception);
            }

            for (size_t i = 0; i < failure_count; i++) {
                FailureGuard failure_guard{ctx};
                failure_guard.failure =
                    impl::run_result_failure(ctx, result, i);
                if (!quiet) {
                    std::fprintf(stderr, "\nFailure %zu of %zu:\n", i + 1,
                                 failure_count);
                }
                handle_failure(test_fn, ctx, s, settings, location, rerun_form,
                               failure_guard.failure);
            }
            throw std::runtime_error("Hegel test failed with " +
                                     std::to_string(failure_count) +
                                     " distinct failures");
        }
    } // namespace

    namespace internal {
        namespace {
            // Function-local static so registrations from other translation
            // units' static initializers always find a constructed registry.
            std::map<std::string, std::string>& blob_registry() {
                static std::map<std::string, std::string> registry;
                return registry;
            }
        } // namespace

        bool install_framework_hooks(const FrameworkHooks& hooks) {
            framework_hooks() = hooks;
            return true;
        }

        CapturedException capture_current_exception() {
            std::exception_ptr exception = std::current_exception();
            try {
                throw;
            } catch (const std::exception& error) {
                std::string type = demangle(typeid(error).name());
                return {exception,
                        origin_of(dynamic_cast<const FailureOrigin*>(&error),
                                  type)};
            } catch (...) {
                std::string type = "unknown_exception";
                if (const std::type_info* info =
                        abi::__cxa_current_exception_type()) {
                    type = demangle(info->name());
                }
                return {exception, origin_of(nullptr, type)};
            }
        }

        [[noreturn]] void CapturedException::rethrow() const {
            concurrent_exception_origin = origin;
            std::rethrow_exception(exception);
        }

        bool register_blob(const char* name, std::vector<const char*> blobs) {
            blob_registry().insert({name, blobs.front()});
            return true;
        }

        std::vector<std::string> reproduce_blobs_for(const char* name) {
            auto blob = blob_registry().find(name);
            if (blob == blob_registry().end()) {
                return {};
            }
            return {blob->second};
        }
    } // namespace internal

    namespace {
        void run_test(const std::function<void(TestCase&)>& test_fn,
                      const std::optional<TestLocation>& location,
                      RerunForm rerun_form, const Settings& settings,
                      const std::vector<std::string>& failure_blobs) {
            hegel_context_t* ctx = impl::thread_context();

            SettingsGuard settings_guard{ctx};
            settings_guard.s = impl::settings_new(ctx);
            hegel_settings_t* s = settings_guard.s;
            apply_settings(ctx, s, settings);

            if (failure_blobs.empty()) {
                run_from_engine(test_fn, ctx, s, settings, location,
                                rerun_form);
            } else {
                run_from_blob(test_fn, ctx, s, settings, failure_blobs);
            }
        }
    } // namespace

    void test(const std::function<void(TestCase&)>& test_fn,
              const Settings& settings,
              const std::vector<std::string>& failure_blobs,
              const char* caller_file, const char* caller_function,
              int caller_line) {
        std::string file = caller_file == nullptr ? "" : caller_file;
        std::string framework_name = framework_test_name();
        std::optional<TestLocation> location;
        if (!framework_name.empty()) {
            location = TestLocation{framework_name, file, caller_line};
        }
        std::string key_name = framework_name;
        if (key_name.empty() && caller_function != nullptr) {
            key_name = caller_function;
        }
        run_test(test_fn, location, RerunForm::Call,
                 with_default_database_key(settings, caller_file, key_name),
                 failure_blobs);
    }

    void test(const std::function<void(TestCase&)>& test_fn,
              const TestLocation& location, const Settings& settings,
              const std::vector<std::string>& failure_blobs) {
        run_test(test_fn, location, RerunForm::Call,
                 with_default_database_key(settings, location.file.c_str(),
                                           location.name),
                 failure_blobs);
    }

    namespace internal {
        void test_from_macro(const std::function<void(TestCase&)>& test_fn,
                             const TestLocation& location,
                             const Settings& settings,
                             const std::vector<std::string>& failure_blobs) {
            run_test(test_fn, location, RerunForm::Annotation, settings,
                     failure_blobs);
        }
    } // namespace internal
} // namespace hegel
