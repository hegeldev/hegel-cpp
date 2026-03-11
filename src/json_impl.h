#pragma once

#include <hegel/json.h>
#include <nlohmann/json.hpp>

namespace hegel::internal::json {
    struct json_ref_holder {
        nlohmann::json& data;

        json_ref_holder(nlohmann::json& ref_) : data(ref_) {}
    };

    struct json_holder {
        nlohmann::json data;

        json_holder() : data() {}
        json_holder(const nlohmann::json& init) : data(init) {}
        json_holder(nlohmann::json&& init) : data(std::move(init)) {}
    };

    class ImplUtil {
      public:
        static nlohmann::json& raw(json_raw_ref ref) { return ref.ref->data; }
        static const nlohmann::json& raw(const json& json) {
            return json.impl->data;
        }
        static json create(const nlohmann::json& from) {
            json result(nullptr);
            result.impl.reset(new json_holder(from));
            return result;
        }
    };
} // namespace hegel::internal::json
