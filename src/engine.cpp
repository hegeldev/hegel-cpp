#include <engine.h>

#include <hegel/internal.h>
#include <hegel/json.h>
#include <hegel/test_case.h>

#include "json_impl.h"

#include <hegel.h>
#include <protocol.h>
#include <test_case.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace hegel::impl {
    // GCOVR_EXCL_START

    std::string last_error(hegel_context_t* ctx) {
        const char* msg = hegel_context_last_error(ctx);
        return msg ? std::string(msg) : std::string();
    }

    namespace {

        // Translate a libhegel return code into success or a thrown
        // diagnostic, deriving the label from the code and appending the
        // context's last-error message.
        void check_rc(hegel_context_t* ctx, hegel_result_t rc) {
            if (rc == HEGEL_OK) {
                return;
            }
            const char* label;
            switch (rc) {
            case HEGEL_E_STOP_TEST:
                label = "engine stopped test";
                break;
            case HEGEL_E_ASSUME:
                label = "assumption rejected";
                break;
            case HEGEL_E_BACKEND:
                label = "backend error";
                break;
            case HEGEL_E_INVALID_HANDLE:
                label = "invalid handle";
                break;
            case HEGEL_E_INVALID_ARG:
                label = "invalid argument";
                break;
            case HEGEL_E_ALREADY_COMPLETE:
                label = "test case already complete";
                break;
            case HEGEL_E_NOT_COMPLETE:
                label = "previous test case not complete";
                break;
            case HEGEL_E_INTERNAL:
                label = "internal error";
                break;
            default:
                label = "unknown error";
                break;
            }
            std::string msg = last_error(ctx);
            throw std::runtime_error(std::string(label) +
                                     (msg.empty() ? "" : ": " + msg));
        }

    } // namespace

    void settings_set_test_cases(hegel_context_t* ctx, hegel_settings_t* s,
                                 uint64_t test_cases) {
        check_rc(ctx, hegel_settings_set_test_cases(ctx, s, test_cases));
    }

    void settings_set_verbosity(hegel_context_t* ctx, hegel_settings_t* s,
                                hegel_verbosity_t verbosity) {
        check_rc(ctx, hegel_settings_set_verbosity(ctx, s, verbosity));
    }

    void settings_set_seed(hegel_context_t* ctx, hegel_settings_t* s,
                           uint64_t seed, bool has_seed) {
        check_rc(ctx, hegel_settings_set_seed(ctx, s, seed, has_seed));
    }

    void settings_set_derandomize(hegel_context_t* ctx, hegel_settings_t* s,
                                  bool derandomize) {
        check_rc(ctx, hegel_settings_set_derandomize(ctx, s, derandomize));
    }

    void settings_set_database(hegel_context_t* ctx, hegel_settings_t* s,
                               const char* path) {
        check_rc(ctx, hegel_settings_set_database(ctx, s, path));
    }

    void settings_set_suppress_health_check(hegel_context_t* ctx,
                                            hegel_settings_t* s,
                                            uint32_t mask) {
        check_rc(ctx, hegel_settings_set_suppress_health_check(ctx, s, mask));
    }

    hegel_settings_t* settings_new(hegel_context_t* ctx) {
        hegel_settings_t* s = nullptr;
        check_rc(ctx, hegel_settings_new(ctx, &s));
        return s;
    }

    hegel_run_t* run_start(hegel_context_t* ctx, hegel_settings_t* s) {
        hegel_run_t* run = nullptr;
        check_rc(ctx, hegel_run_start(ctx, s, &run));
        return run;
    }

    hegel_test_case_t* test_case_from_blob(hegel_context_t* ctx,
                                           hegel_settings_t* s,
                                           const char* blob) {
        hegel_test_case_t* tc = nullptr;
        check_rc(ctx, hegel_test_case_from_blob(ctx, s, blob, &tc));
        return tc;
    }

    hegel_test_case_t* next_test_case(hegel_context_t* ctx, hegel_run_t* run) {
        hegel_test_case_t* tc = nullptr;
        check_rc(ctx, hegel_next_test_case(ctx, run, &tc));
        return tc;
    }

    void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                       hegel_status_t status, const char* origin) {
        check_rc(ctx, hegel_mark_complete(ctx, tc, status, origin));
    }

    const hegel_run_result_t* run_result(hegel_context_t* ctx,
                                         hegel_run_t* run) {
        const hegel_run_result_t* result = nullptr;
        check_rc(ctx, hegel_run_result(ctx, run, &result));
        return result;
    }

    hegel_run_status_t run_result_status(hegel_context_t* ctx,
                                         const hegel_run_result_t* result) {
        hegel_run_status_t status = HEGEL_RUN_STATUS_PASSED;
        check_rc(ctx, hegel_run_result_status(ctx, result, &status));
        return status;
    }

    const char* run_result_error(hegel_context_t* ctx,
                                 const hegel_run_result_t* result) {
        const char* err = nullptr;
        check_rc(ctx, hegel_run_result_error(ctx, result, &err));
        return err;
    }

    size_t run_result_failure_count(hegel_context_t* ctx,
                                    const hegel_run_result_t* result) {
        size_t count = 0;
        check_rc(ctx, hegel_run_result_failure_count(ctx, result, &count));
        return count;
    }

    const hegel_failure_t* run_result_failure(hegel_context_t* ctx,
                                              const hegel_run_result_t* result,
                                              size_t index) {
        const hegel_failure_t* failure = nullptr;
        check_rc(ctx, hegel_run_result_failure(ctx, result, index, &failure));
        return failure;
    }

    const char* failure_reproduction_blob(hegel_context_t* ctx,
                                          const hegel_failure_t* failure) {
        const char* blob = nullptr;
        check_rc(ctx, hegel_failure_reproduction_blob(ctx, failure, &blob));
        return blob;
    }

    // GCOVR_EXCL_STOP

} // namespace hegel::impl

namespace hegel::internal {

    // Draw a single value: hand the CBOR schema to libhegel's in-process
    // engine via `hegel_generate` and decode the CBOR value it returns.
    // Returns `{"result": <value>}` so callers (BasicGenerator::do_draw,
    // HegelRandom) can keep reading `response["result"]`.
    hegel::internal::json::json
    generate_from_schema(const hegel::internal::json::json& schema,
                         const hegel::TestCase& tc) {
        auto* data = tc.data();
        hegel_context_t* ctx = data->ctx;
        hegel_test_case_t* htc = data->tc;

        const nlohmann::json& schema_raw = json::ImplUtil::raw(schema);
        std::vector<uint8_t> schema_cbor =
            impl::protocol::cbor_encode(schema_raw);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "REQUEST: " << schema_raw.dump() << "\n";
        }

        const uint8_t* out_value = nullptr;
        size_t out_len = 0;
        hegel_result_t rc =
            hegel_generate(ctx, htc, schema_cbor.data(), schema_cbor.size(),
                           &out_value, &out_len);

        // Engine ran out of choice budget for this case: abandon the body.
        // The runner marks the case OVERRUN.
        if (rc == HEGEL_E_STOP_TEST) {
            throw HegelStopTest();
        }
        // A precondition (engine-side filter / assume) rejected this draw.
        if (rc == HEGEL_E_ASSUME) {
            throw HegelReject();
        }
        if (rc != HEGEL_OK) {
            // Engine-level failure; not reachable without fault injection into
            // the C ABI.
            // GCOVR_EXCL_START
            throw std::runtime_error("hegel_generate failed: " +
                                     impl::last_error(ctx));
            // GCOVR_EXCL_STOP
        }

        nlohmann::json value = impl::protocol::cbor_decode(out_value, out_len);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "RESPONSE: " << value.dump() << "\n";
        }
        if (data->should_log()) {
            std::cerr << "Generated: " << value.dump() << "\n";
        }

        nlohmann::json response;
        response["result"] = std::move(value);
        return json::ImplUtil::create(response);
    }

} // namespace hegel::internal
