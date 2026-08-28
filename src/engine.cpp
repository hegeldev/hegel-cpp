#include <engine.h>

#include <hegel/internal.h>
#include <hegel/test_case.h>

#include <hegel.h>
#include <test_case.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace hegel::impl {

    hegel_context_t* thread_context() {
        struct ThreadContext {
            hegel_context_t* ctx = hegel_context_new();
            ~ThreadContext() { hegel_context_free(ctx); }
        };
        thread_local ThreadContext context;
        return context.ctx;
    }

    // GCOVR_EXCL_START

    std::string last_error(hegel_context_t* ctx) {
        const char* msg = hegel_context_last_error(ctx);
        return msg ? std::string(msg) : std::string();
    }

    namespace {

        const char* rc_label(hegel_result_t rc) {
            switch (rc) {
            case HEGEL_E_STOP_TEST:
                return "engine stopped test";
            case HEGEL_E_ASSUME:
                return "assumption rejected";
            case HEGEL_E_BACKEND:
                return "backend error";
            case HEGEL_E_INVALID_HANDLE:
                return "invalid handle";
            case HEGEL_E_INVALID_ARG:
                return "invalid argument";
            case HEGEL_E_ALREADY_COMPLETE:
                return "test case already complete";
            case HEGEL_E_NOT_COMPLETE:
                return "previous test case not complete";
            case HEGEL_E_INTERNAL:
                return "internal error";
            case HEGEL_E_CONCURRENT_USE:
                return "concurrent use of test case handle";
            default:
                return "unknown error";
            }
        }

        // Translate a libhegel return code into success or a thrown
        // diagnostic, deriving the label from the code and appending the
        // context's last-error message.
        void check_rc(hegel_context_t* ctx, hegel_result_t rc) {
            if (rc == HEGEL_OK) {
                return;
            }
            std::string msg = last_error(ctx);
            throw std::runtime_error(std::string(rc_label(rc)) +
                                     (msg.empty() ? "" : ": " + msg));
        }

    } // namespace

    void settings_set_test_cases(hegel_context_t* ctx, hegel_settings_t* s,
                                 uint64_t test_cases) {
        check_rc(ctx, hegel_settings_set_test_cases(ctx, s, test_cases));
    }

    void settings_set_stateful_step_count(hegel_context_t* ctx,
                                          hegel_settings_t* s, int64_t n) {
        check_rc(ctx, hegel_settings_set_stateful_step_count(ctx, s, n));
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

    void settings_set_report_multiple_failures(hegel_context_t* ctx,
                                               hegel_settings_t* s, bool yes) {
        check_rc(ctx, hegel_settings_set_report_multiple_failures(ctx, s, yes));
    }

    void settings_set_database(hegel_context_t* ctx, hegel_settings_t* s,
                               const char* path) {
        check_rc(ctx, hegel_settings_set_database(ctx, s, path));
    }

    void settings_set_database_key(hegel_context_t* ctx, hegel_settings_t* s,
                                   const char* key) {
        check_rc(ctx, hegel_settings_set_database_key(ctx, s, key));
    }

    void settings_set_suppress_health_check(hegel_context_t* ctx,
                                            hegel_settings_t* s,
                                            uint32_t mask) {
        check_rc(ctx, hegel_settings_set_suppress_health_check(ctx, s, mask));
    }

    void settings_set_phases(hegel_context_t* ctx, hegel_settings_t* s,
                             uint32_t phases) {
        check_rc(ctx, hegel_settings_set_phases(ctx, s, phases));
    }

    void settings_set_mode(hegel_context_t* ctx, hegel_settings_t* s,
                           hegel_mode_t mode) {
        check_rc(ctx, hegel_settings_set_mode(ctx, s, mode));
    }

    void settings_set_backend(hegel_context_t* ctx, hegel_settings_t* s,
                              hegel_backend_t backend) {
        check_rc(ctx, hegel_settings_set_backend(ctx, s, backend));
    }

    hegel_settings_t* settings_new(hegel_context_t* ctx) {
        hegel_settings_t* s = nullptr;
        check_rc(ctx, hegel_settings_new(ctx, &s));
        return s;
    }

    hegel_run_t* run_start(hegel_context_t* ctx, hegel_settings_t* s) {
        hegel_run_t* run = nullptr;
        check_rc(ctx, hegel_run_start(ctx, s, nullptr, nullptr, &run));
        return run;
    }

    hegel_test_case_t* test_case_from_blob(hegel_context_t* ctx,
                                           hegel_settings_t* s,
                                           const char* blob) {
        hegel_test_case_t* tc = nullptr;
        check_rc(ctx, hegel_test_case_from_blob(ctx, s, blob, nullptr, nullptr,
                                                &tc));
        return tc;
    }

    hegel_test_case_t* next_test_case(hegel_context_t* ctx, hegel_run_t* run) {
        hegel_test_case_t* tc = nullptr;
        check_rc(ctx, hegel_next_test_case(ctx, run, &tc));
        return tc;
    }

    hegel_test_case_t* test_case_clone(hegel_context_t* ctx,
                                       const hegel_test_case_t* tc) {
        hegel_test_case_t* clone = nullptr;
        check_rc(ctx, hegel_test_case_clone(ctx, tc, &clone));
        return clone;
    }

    void mark_complete(hegel_context_t* ctx, hegel_test_case_t* tc,
                       hegel_status_t status, const char* origin) {
        check_rc(ctx, hegel_mark_complete(ctx, tc, status, origin));
    }

    hegel_run_result_t* run_result(hegel_context_t* ctx, hegel_run_t* run) {
        hegel_run_result_t* result = nullptr;
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

    hegel_failure_t* run_result_failure(hegel_context_t* ctx,
                                        const hegel_run_result_t* result,
                                        size_t index) {
        hegel_failure_t* failure = nullptr;
        check_rc(ctx, hegel_run_result_failure(ctx, result, index, &failure));
        return failure;
    }

    const char* failure_reproduction_blob(hegel_context_t* ctx,
                                          const hegel_failure_t* failure) {
        const char* blob = nullptr;
        check_rc(ctx, hegel_failure_reproduction_blob(ctx, failure, &blob));
        return blob;
    }

    const char* failure_origin(hegel_context_t* ctx,
                               const hegel_failure_t* failure) {
        const char* origin = nullptr;
        check_rc(ctx, hegel_failure_origin(ctx, failure, &origin));
        return origin;
    }

    // GCOVR_EXCL_STOP

    namespace {
        struct DrawScope {
            hegel_context_t* ctx;
            hegel_test_case_t* tc;

            explicit DrawScope(const TestCase& obj) {
                ctx = thread_context();
                tc = obj.data()->tc;
            }

            // Route a libhegel return code to success or the matching
            // exception.
            void raise_for_rc(hegel_result_t rc, const char* what) const {
                if (rc == HEGEL_OK) {
                    return;
                }
                if (rc == HEGEL_E_STOP_TEST) {
                    throw internal::HegelStopTest();
                }
                if (rc == HEGEL_E_ASSUME) {
                    throw internal::HegelReject();
                }
                if (rc == HEGEL_E_INVALID_ARG) {
                    throw std::invalid_argument(last_error(ctx));
                }
                // Engine-level failure; not reachable without fault
                // injection into the C ABI.
                // GCOVR_EXCL_START
                std::string msg = last_error(ctx);
                throw std::runtime_error(std::string(what) + " failed (" +
                                         rc_label(rc) +
                                         (msg.empty() ? ")" : "): " + msg));
                // GCOVR_EXCL_STOP
            }
        };

        // RAII for the engine-allocated draw buffers.
        struct BytesResultGuard {
            hegel_context_t* ctx;
            hegel_generate_bytes_result_t result{nullptr, 0};
            ~BytesResultGuard() {
                hegel_generate_bytes_result_free(ctx, &result);
            }
        };

        struct StringResultGuard {
            hegel_context_t* ctx;
            hegel_generate_string_result_t result{nullptr, 0};
            ~StringResultGuard() {
                hegel_generate_string_result_free(ctx, &result);
            }
        };

        // Construct a string generator, converting an engine rejection into
        // std::invalid_argument at generator-construction time.
        template <typename F>
        hegel_string_generator_t* make_string_generator(const char* what,
                                                        F&& build) {
            hegel_context_t* ctx = thread_context();
            hegel_string_generator_t* generator = nullptr;
            hegel_result_t rc = build(ctx, &generator);
            if (rc != HEGEL_OK) {
                std::string msg = last_error(ctx);
                throw std::invalid_argument(
                    std::string(what) +
                    (msg.empty() ? ": invalid arguments" : ": " + msg));
            }
            return generator;
        }

    } // namespace

    hegel_string_generator_t*
    string_generator_text(const TextGeneratorParams& params) {
        std::vector<const char*> categories;
        std::vector<const char*> exclude_categories;
        if (params.categories) {
            for (const auto& c : *params.categories) {
                categories.push_back(c.c_str());
            }
        }
        if (params.exclude_categories) {
            for (const auto& c : *params.exclude_categories) {
                exclude_categories.push_back(c.c_str());
            }
        }
        auto chars = [](const std::string* s) {
            return s ? reinterpret_cast<const uint8_t*>(s->data()) : nullptr;
        };
        return make_string_generator(
            "text", [&](hegel_context_t* ctx, hegel_string_generator_t** out) {
                return hegel_string_generator_text(
                    ctx, params.min_size, params.max_size, params.codec,
                    params.min_codepoint, params.max_codepoint,
                    params.categories ? categories.data() : nullptr,
                    categories.size(),
                    params.exclude_categories ? exclude_categories.data()
                                              : nullptr,
                    exclude_categories.size(), chars(params.include_characters),
                    params.include_characters
                        ? params.include_characters->size()
                        : 0,
                    chars(params.exclude_characters),
                    params.exclude_characters
                        ? params.exclude_characters->size()
                        : 0,
                    out);
            });
    }

    hegel_string_generator_t* string_generator_regex(const char* pattern,
                                                     bool fullmatch) {
        return make_string_generator(
            "from_regex",
            [&](hegel_context_t* ctx, hegel_string_generator_t** out) {
                return hegel_string_generator_regex(ctx, pattern, fullmatch,
                                                    nullptr, out);
            });
    }

    hegel_string_generator_t* string_generator_email() {
        return make_string_generator(
            "emails", [](hegel_context_t* ctx, hegel_string_generator_t** out) {
                return hegel_string_generator_email(ctx, out);
            });
    }

    hegel_string_generator_t* string_generator_url() {
        return make_string_generator(
            "urls", [](hegel_context_t* ctx, hegel_string_generator_t** out) {
                return hegel_string_generator_url(ctx, out);
            });
    }

    hegel_string_generator_t* string_generator_domain(uint64_t max_length) {
        return make_string_generator(
            "domains",
            [&](hegel_context_t* ctx, hegel_string_generator_t** out) {
                return hegel_string_generator_domain(ctx, max_length, out);
            });
    }

    void string_generator_free(hegel_string_generator_t* generator) {
        hegel_string_generator_free(thread_context(), generator);
    }

    std::string draw_string(const TestCase& tc,
                            const hegel_string_generator_t* generator) {
        DrawScope scope(tc);
        StringResultGuard guard{scope.ctx};
        scope.raise_for_rc(hegel_generate_string(scope.ctx, scope.tc, generator,
                                                 &guard.result),
                           "hegel_generate_string");
        std::string value(guard.result.data, guard.result.len);
        return value;
    }

    std::vector<uint8_t> draw_bytes(const TestCase& tc, uint64_t min_size,
                                    uint64_t max_size) {
        DrawScope scope(tc);
        BytesResultGuard guard{scope.ctx};
        scope.raise_for_rc(hegel_generate_bytes(scope.ctx, scope.tc, min_size,
                                                max_size, &guard.result),
                           "hegel_generate_bytes");
        std::vector<uint8_t> value(guard.result.data,
                                   guard.result.data + guard.result.len);
        return value;
    }

    hegel_date_t draw_date(const TestCase& tc) {
        DrawScope scope(tc);
        hegel_date_t value{};
        scope.raise_for_rc(
            hegel_generate_date(scope.ctx, scope.tc, hegel_date_t{1, 1, 1},
                                hegel_date_t{9999, 12, 31}, &value),
            "hegel_generate_date");
        return value;
    }

    hegel_time_t draw_time(const TestCase& tc) {
        DrawScope scope(tc);
        hegel_time_t value{};
        scope.raise_for_rc(
            hegel_generate_time(scope.ctx, scope.tc, hegel_time_t{0, 0, 0, 0},
                                hegel_time_t{23, 59, 59, 999999}, &value),
            "hegel_generate_time");
        return value;
    }

    hegel_datetime_t draw_datetime(const TestCase& tc) {
        DrawScope scope(tc);
        hegel_datetime_t value{};
        hegel_datetime_t min_value{{1, 1, 1}, {0, 0, 0, 0}};
        hegel_datetime_t max_value{{9999, 12, 31}, {23, 59, 59, 999999}};
        scope.raise_for_rc(hegel_generate_datetime(scope.ctx, scope.tc,
                                                   min_value, max_value,
                                                   &value),
                           "hegel_generate_datetime");
        return value;
    }

    std::string draw_ipv4(const TestCase& tc) {
        DrawScope scope(tc);
        std::array<uint8_t, 4> bytes{};
        scope.raise_for_rc(
            hegel_generate_ipv4(scope.ctx, scope.tc, bytes.data()),
            "hegel_generate_ipv4");
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, bytes.data(), buf, sizeof(buf)) == nullptr) {
            // 4 bytes always format; not reachable.
            // GCOVR_EXCL_START
            throw std::runtime_error("failed to format IPv4 address");
            // GCOVR_EXCL_STOP
        }
        return buf;
    }

    std::string draw_ipv6(const TestCase& tc) {
        DrawScope scope(tc);
        std::array<uint8_t, 16> bytes{};
        scope.raise_for_rc(
            hegel_generate_ipv6(scope.ctx, scope.tc, bytes.data()),
            "hegel_generate_ipv6");
        char buf[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, bytes.data(), buf, sizeof(buf)) == nullptr) {
            // 16 bytes always format; not reachable.
            // GCOVR_EXCL_START
            throw std::runtime_error("failed to format IPv6 address");
            // GCOVR_EXCL_STOP
        }
        return buf;
    }

    std::string draw_uuid(const TestCase& tc, std::optional<uint8_t> version) {
        DrawScope scope(tc);
        std::array<uint8_t, 16> bytes{};
        scope.raise_for_rc(
            hegel_generate_uuid(scope.ctx, scope.tc, version.value_or(0),
                                version.has_value(), bytes.data()),
            "hegel_generate_uuid");
        // Canonical 8-4-4-4-12 hex, with hyphens before bytes 4, 6, 8, 10.
        static constexpr char kHex[] = "0123456789abcdef";
        std::string value;
        value.reserve(36);
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                value.push_back('-');
            }
            value.push_back(kHex[bytes[i] >> 4]);
            value.push_back(kHex[bytes[i] & 0x0F]);
        }
        return value;
    }

    void target(const TestCase& tc, double score, const char* label) {
        DrawScope scope(tc);
        scope.raise_for_rc(hegel_target(scope.ctx, scope.tc, score, label),
                           "hegel_target");
    }

} // namespace hegel::impl

namespace hegel::internal {

    namespace {

        // The public SpanLabel enum mirrors the C ABI's label values so the
        // public headers don't need the C header.
        static_assert(static_cast<uint64_t>(SpanLabel::List) ==
                      HEGEL_LABEL_LIST);
        static_assert(static_cast<uint64_t>(SpanLabel::ListElement) ==
                      HEGEL_LABEL_LIST_ELEMENT);
        static_assert(static_cast<uint64_t>(SpanLabel::Set) == HEGEL_LABEL_SET);
        static_assert(static_cast<uint64_t>(SpanLabel::SetElement) ==
                      HEGEL_LABEL_SET_ELEMENT);
        static_assert(static_cast<uint64_t>(SpanLabel::Map) == HEGEL_LABEL_MAP);
        static_assert(static_cast<uint64_t>(SpanLabel::MapEntry) ==
                      HEGEL_LABEL_MAP_ENTRY);
        static_assert(static_cast<uint64_t>(SpanLabel::Tuple) ==
                      HEGEL_LABEL_TUPLE);
        static_assert(static_cast<uint64_t>(SpanLabel::OneOf) ==
                      HEGEL_LABEL_ONE_OF);
        static_assert(static_cast<uint64_t>(SpanLabel::Optional) ==
                      HEGEL_LABEL_OPTIONAL);
        static_assert(static_cast<uint64_t>(SpanLabel::FlatMap) ==
                      HEGEL_LABEL_FLAT_MAP);
        static_assert(static_cast<uint64_t>(SpanLabel::Filter) ==
                      HEGEL_LABEL_FILTER);
        static_assert(static_cast<uint64_t>(SpanLabel::Mapped) ==
                      HEGEL_LABEL_MAPPED);
        static_assert(static_cast<uint64_t>(SpanLabel::SampledFrom) ==
                      HEGEL_LABEL_SAMPLED_FROM);
        static_assert(static_cast<uint64_t>(SpanLabel::EnumVariant) ==
                      HEGEL_LABEL_ENUM_VARIANT);
        static_assert(static_cast<uint64_t>(SpanLabel::StatefulRule) ==
                      HEGEL_LABEL_STATEFUL_RULE);
        static_assert(state_machine_done == HEGEL_STATE_MACHINE_DONE);

        // Two's-complement little-endian encoding of a uint64_t for the
        // engine's big-integer draw: 9 bytes so the top bit is never read
        // as a sign.
        constexpr size_t big_u64_len = 9;

        void encode_u64_le(uint64_t value, uint8_t* out) {
            for (size_t i = 0; i < 8; ++i) {
                out[i] = static_cast<uint8_t>(value >> (8 * i));
            }
            out[8] = 0;
        }

        uint64_t decode_u64_le(const uint8_t* in) {
            uint64_t value = 0;
            for (size_t i = 0; i < 8; ++i) {
                value |= static_cast<uint64_t>(in[i]) << (8 * i);
            }
            return value;
        }

    } // namespace

    void start_span(const TestCase& tc, SpanLabel label) {
        impl::DrawScope scope(tc);
        scope.raise_for_rc(
            hegel_start_span(scope.ctx, scope.tc, static_cast<uint64_t>(label)),
            "hegel_start_span");
    }

    void stop_span(const TestCase& tc, bool discard) {
        impl::DrawScope scope(tc);
        scope.raise_for_rc(hegel_stop_span(scope.ctx, scope.tc, discard),
                           "hegel_stop_span");
    }

    int64_t draw_integer(const TestCase& tc, int64_t min_value,
                         int64_t max_value) {
        impl::DrawScope scope(tc);
        int64_t value = 0;
        scope.raise_for_rc(hegel_generate_integer(scope.ctx, scope.tc,
                                                  min_value, max_value, &value),
                           "hegel_generate_integer");
        return value;
    }

    uint64_t draw_integer_unsigned(const TestCase& tc, uint64_t min_value,
                                   uint64_t max_value) {
        if (max_value <=
            static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return static_cast<uint64_t>(
                draw_integer(tc, static_cast<int64_t>(min_value),
                             static_cast<int64_t>(max_value)));
        }
        impl::DrawScope scope(tc);
        uint8_t min_bytes[big_u64_len];
        uint8_t max_bytes[big_u64_len];
        uint8_t out_bytes[big_u64_len];
        encode_u64_le(min_value, min_bytes);
        encode_u64_le(max_value, max_bytes);
        size_t out_len = 0;
        scope.raise_for_rc(
            hegel_generate_integer_big(scope.ctx, scope.tc, min_bytes,
                                       big_u64_len, max_bytes, big_u64_len,
                                       out_bytes, big_u64_len, &out_len),
            "hegel_generate_integer_big");
        uint64_t value = decode_u64_le(out_bytes);
        return value;
    }

    bool draw_boolean(const TestCase& tc, double p,
                      std::optional<bool> forced) {
        impl::DrawScope scope(tc);
        bool value = false;
        scope.raise_for_rc(
            hegel_generate_boolean(scope.ctx, scope.tc, p,
                                   /*forced=*/forced.value_or(false),
                                   /*has_forced=*/forced.has_value(), &value),
            "hegel_generate_boolean");
        return value;
    }

    double draw_float(const TestCase& tc, uint32_t width, double min_value,
                      double max_value, bool allow_nan, bool allow_infinity,
                      bool exclude_min, bool exclude_max,
                      double smallest_nonzero_magnitude) {
        impl::DrawScope scope(tc);
        double value = 0.0;
        scope.raise_for_rc(hegel_generate_float(
                               scope.ctx, scope.tc, width, min_value, max_value,
                               allow_nan, allow_infinity, exclude_min,
                               exclude_max, smallest_nonzero_magnitude, &value),
                           "hegel_generate_float");
        return value;
    }

    CollectionHandle::CollectionHandle(const TestCase& tc, uint64_t min_size,
                                       uint64_t max_size) {
        impl::DrawScope scope(tc);
        scope.raise_for_rc(hegel_new_collection(scope.ctx, scope.tc, min_size,
                                                max_size, &handle_),
                           "hegel_new_collection");
    }

    CollectionHandle::~CollectionHandle() {
        hegel_collection_free(impl::thread_context(), handle_);
    }

    bool CollectionHandle::more(const TestCase& tc) {
        impl::DrawScope scope(tc);
        bool more = false;
        scope.raise_for_rc(
            hegel_collection_more(scope.ctx, scope.tc, handle_, &more),
            "hegel_collection_more");
        return more;
    }

    void CollectionHandle::reject(const TestCase& tc, const char* why) {
        impl::DrawScope scope(tc);
        scope.raise_for_rc(
            hegel_collection_reject(scope.ctx, scope.tc, handle_, why),
            "hegel_collection_reject");
    }

    PoolHandle::PoolHandle(const TestCase& tc) {
        impl::DrawScope scope(tc);
        scope.raise_for_rc(hegel_new_pool(scope.ctx, scope.tc, &handle_),
                           "hegel_new_pool");
    }

    PoolHandle::~PoolHandle() {
        hegel_pool_free(impl::thread_context(), handle_);
    }

    int64_t PoolHandle::add(const TestCase& tc) {
        impl::DrawScope scope(tc);
        int64_t var_id = 0;
        scope.raise_for_rc(
            hegel_pool_add(scope.ctx, scope.tc, handle_, &var_id),
            "hegel_pool_add");
        return var_id;
    }

    int64_t PoolHandle::draw_variable(const TestCase& tc, bool consume) {
        impl::DrawScope scope(tc);
        int64_t var_id = 0;
        scope.raise_for_rc(
            hegel_pool_generate(scope.ctx, scope.tc, handle_, consume, &var_id),
            "hegel_pool_generate");
        return var_id;
    }

    StateMachineHandle::StateMachineHandle(
        const TestCase& tc, const std::vector<std::string>& rule_names,
        const std::vector<std::string>& invariant_names) {
        impl::DrawScope scope(tc);

        auto to_cstrings = [](const std::vector<std::string>& names) {
            std::vector<char*> cstrings;
            cstrings.reserve(names.size());
            for (const std::string& name : names)
                cstrings.push_back(const_cast<char*>(name.c_str()));
            return cstrings;
        };

        std::vector<char*> rule_name_cstrings = to_cstrings(rule_names);
        std::vector<char*> invariant_name_cstrings =
            to_cstrings(invariant_names);

        scope.raise_for_rc(hegel_new_state_machine(
                               scope.ctx, scope.tc, rule_name_cstrings.data(),
                               rule_names.size(),
                               invariant_name_cstrings.data(),
                               invariant_names.size(), &handle_),
                           "hegel_new_state_machine");
    }

    StateMachineHandle::~StateMachineHandle() {
        hegel_state_machine_free(impl::thread_context(), handle_);
    }

    int64_t StateMachineHandle::next_rule(const TestCase& tc) {
        impl::DrawScope scope(tc);
        int64_t rule_idx;

        scope.raise_for_rc(hegel_state_machine_next_rule(scope.ctx, scope.tc,
                                                         handle_, &rule_idx),
                           "hegel_state_machine_next_rule");
        return rule_idx;
    }

    void StateMachineHandle::rule_rejected(const TestCase& tc) {
        impl::DrawScope scope(tc);
        scope.raise_for_rc(
            hegel_state_machine_rule_rejected(scope.ctx, scope.tc, handle_),
            "hegel_state_machine_rule_rejected");
    }

} // namespace hegel::internal
