#pragma once

/**
 * @cond INTERNAL
 *
 * Thin helpers over the libhegel C ABI shared by the run loop (hegel.cpp)
 * and the per-draw path (engine.cpp).
 */

#include <cstddef>
#include <cstdint>
#include <hegel.h>
#include <string>

namespace hegel::impl {

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
    void settings_set_database(hegel_context_t* ctx, hegel_settings_t* s,
                               const char* path);
    void settings_set_suppress_health_check(hegel_context_t* ctx,
                                            hegel_settings_t* s, uint32_t mask);
    hegel_settings_t* settings_new(hegel_context_t* ctx);

    hegel_run_t* run_start(hegel_context_t* ctx, hegel_settings_t* s);
    hegel_test_case_t* test_case_from_blob(hegel_context_t* ctx,
                                           hegel_settings_t* s,
                                           const char* blob);
    /// NULL once the engine has no more cases to hand out.
    hegel_test_case_t* next_test_case(hegel_context_t* ctx, hegel_run_t* run);
    void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                       hegel_status_t status, const char* origin);

    const hegel_run_result_t* run_result(hegel_context_t* ctx,
                                         hegel_run_t* run);
    hegel_run_status_t run_result_status(hegel_context_t* ctx,
                                         const hegel_run_result_t* result);
    const char* run_result_error(hegel_context_t* ctx,
                                 const hegel_run_result_t* result);
    size_t run_result_failure_count(hegel_context_t* ctx,
                                    const hegel_run_result_t* result);
    const hegel_failure_t* run_result_failure(hegel_context_t* ctx,
                                              const hegel_run_result_t* result,
                                              size_t index);
    const char* failure_reproduction_blob(hegel_context_t* ctx,
                                          const hegel_failure_t* failure);

} // namespace hegel::impl

/// @endcond
