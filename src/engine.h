#pragma once

/**
 * @cond INTERNAL
 *
 * Thin helpers over the libhegel C ABI shared by the run loop (hegel.cpp)
 * and the draw primitives (engine.cpp, generators.cpp).
 */

#include <cstddef>
#include <cstdint>
#include <hegel.h>
#include <optional>
#include <string>
#include <vector>

namespace hegel {
    class TestCase;
}

namespace hegel::impl {

    /// This thread's libhegel error-reporting context, created on first use
    /// and freed when the thread exits.
    hegel_context_t* thread_context();

    /// Most recent error message recorded on `ctx` (empty string if none).
    std::string last_error(hegel_context_t* ctx);

    // Safe wrappers over the libhegel C ABI: each forwards to its `hegel_*`
    // entry point and routes the return code through an internal check that
    // throws std::runtime_error (with the context diagnostic) on failure.
    // Out-parameter calls return the produced value directly.

    void settings_set_test_cases(hegel_context_t* ctx, hegel_settings_t* s,
                                 uint64_t test_cases);
    void settings_set_verbosity(hegel_context_t* ctx, hegel_settings_t* s,
                                hegel_verbosity_t verbosity);
    void settings_set_seed(hegel_context_t* ctx, hegel_settings_t* s,
                           uint64_t seed, bool has_seed);
    void settings_set_derandomize(hegel_context_t* ctx, hegel_settings_t* s,
                                  bool derandomize);
    void settings_set_report_multiple_failures(hegel_context_t* ctx,
                                               hegel_settings_t* s, bool yes);
    void settings_set_database(hegel_context_t* ctx, hegel_settings_t* s,
                               const char* path);
    void settings_set_database_key(hegel_context_t* ctx, hegel_settings_t* s,
                                   const char* key);
    void settings_set_suppress_health_check(hegel_context_t* ctx,
                                            hegel_settings_t* s, uint32_t mask);
    void settings_set_phases(hegel_context_t* ctx, hegel_settings_t* s,
                             uint32_t phases);
    void settings_set_mode(hegel_context_t* ctx, hegel_settings_t* s,
                           hegel_mode_t mode);
    void settings_set_backend(hegel_context_t* ctx, hegel_settings_t* s,
                              hegel_backend_t backend);
    hegel_settings_t* settings_new(hegel_context_t* ctx);

    hegel_run_t* run_start(hegel_context_t* ctx, hegel_settings_t* s);
    hegel_test_case_t* test_case_from_blob(hegel_context_t* ctx,
                                           hegel_settings_t* s,
                                           const char* blob);
    /// NULL once the engine has no more cases to hand out. The returned
    /// handle is caller-owned; release it with hegel_test_case_free.
    hegel_test_case_t* next_test_case(hegel_context_t* ctx, hegel_run_t* run);
    void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                       hegel_status_t status, const char* origin);

    /// Caller-owned result snapshot; release with hegel_run_result_free.
    hegel_run_result_t* run_result(hegel_context_t* ctx, hegel_run_t* run);
    hegel_run_status_t run_result_status(hegel_context_t* ctx,
                                         const hegel_run_result_t* result);
    const char* run_result_error(hegel_context_t* ctx,
                                 const hegel_run_result_t* result);
    size_t run_result_failure_count(hegel_context_t* ctx,
                                    const hegel_run_result_t* result);
    /// Caller-owned failure snapshot; release with hegel_failure_free.
    hegel_failure_t* run_result_failure(hegel_context_t* ctx,
                                        const hegel_run_result_t* result,
                                        size_t index);
    const char* failure_reproduction_blob(hegel_context_t* ctx,
                                          const hegel_failure_t* failure);
    /// The origin string the engine grouped this failure's probes under.
    const char* failure_origin(hegel_context_t* ctx,
                               const hegel_failure_t* failure);

    // String-generator construction. Each validates its parameters
    // immediately and throws std::invalid_argument (with the engine's
    // diagnostic) on rejection. The returned handle is immutable, shareable
    // across test cases, and must be released with string_generator_free.

    struct TextGeneratorParams {
        uint64_t min_size = 0;
        uint64_t max_size = 0;
        const char* codec = nullptr; // nullptr = all of Unicode
        uint32_t min_codepoint = 0;
        uint32_t max_codepoint = UINT32_MAX;
        const std::vector<std::string>* categories = nullptr;
        const std::vector<std::string>* exclude_categories = nullptr;
        const std::string* include_characters = nullptr;
        const std::string* exclude_characters = nullptr;
    };

    hegel_string_generator_t*
    string_generator_text(const TextGeneratorParams& params);
    hegel_string_generator_t* string_generator_regex(const char* pattern,
                                                     bool fullmatch);
    hegel_string_generator_t* string_generator_email();
    hegel_string_generator_t* string_generator_url();
    hegel_string_generator_t* string_generator_domain(uint64_t max_length);
    void string_generator_free(hegel_string_generator_t* generator);

    // Draw primitives used by the non-template generators in
    // generators.cpp. (The template-visible ones live in
    // hegel::internal — see include/hegel/internal.h.)

    void target(const TestCase& tc, double score, const char* label);

    std::string draw_string(const TestCase& tc,
                            const hegel_string_generator_t* generator);
    std::vector<uint8_t> draw_bytes(const TestCase& tc, uint64_t min_size,
                                    uint64_t max_size);
    hegel_date_t draw_date(const TestCase& tc);
    hegel_time_t draw_time(const TestCase& tc);
    hegel_datetime_t draw_datetime(const TestCase& tc);
    std::string draw_uuid(const TestCase& tc, std::optional<uint8_t> version);
    /// Dotted-quad (v4) / RFC 5952 colon-hex (v6) text form.
    std::string draw_ipv4(const TestCase& tc);
    std::string draw_ipv6(const TestCase& tc);

} // namespace hegel::impl

/// @endcond
