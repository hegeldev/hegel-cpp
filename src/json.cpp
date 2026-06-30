#include "json_impl.h"
#include <hegel/json.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using hegel::internal::json::ImplUtil;

namespace hegel::internal::json {

    json::json(const json& init) : impl(new json_holder(*init.impl)) {}
    json::json(json&& init) noexcept : impl(std::move(init.impl)) {}

    json::json(initializer_list_t init) {
        // check if each element is an array with two elements whose first
        // element is a string
        bool is_an_object =
            std::all_of(init.begin(), init.end(), [](const json_ref& my_ref) {
                const nlohmann::json& element_ref = (*my_ref).impl->data;

                // The cast is to ensure op[size_type] is called, bearing in
                // mind size_type may not be int; (many string types can be
                // constructed from 0 via its null-pointer guise, so we get a
                // broken call to op[key_type], the wrong semantics and a 4804
                // warning on Windows)
                return element_ref.is_array() && element_ref.size() == 2 &&
                       element_ref[static_cast<size_t>(0)].is_string();
            });

        std::unique_ptr<json_holder> impl(new json_holder);
        if (is_an_object) {
            for (const json_ref& ref : init) {
                const nlohmann::json& elt = (*ref).impl->data;
                const std::string& key =
                    elt[static_cast<size_t>(0)].get_ref<const std::string&>();
                auto& value = elt[static_cast<size_t>(1)];
                (impl->data)[key] = value;
            }
        } else {
            *impl = std::move(*json::array(init).impl);
        }
        this->impl = std::move(impl);
    }

    json::json(const char* init) : impl(new json_holder(init)) {}
    json::json(const int32_t init) : impl(new json_holder(init)) {}
    json::json(const int64_t init) : impl(new json_holder(init)) {}
    json::json(const uint32_t init) : impl(new json_holder(init)) {}
    json::json(const uint64_t init) : impl(new json_holder(init)) {}
#ifdef __APPLE__
    json::json(const unsigned long init)
        : impl(new json_holder(static_cast<uint64_t>(init))) {}
#endif
    json::json(const bool init) : impl(new json_holder(init)) {}
    json::json(const std::string& init) : impl(new json_holder(init)) {}
    json::json(std::nullptr_t init) : impl(new json_holder(init)) {}
    json::~json() = default;

    json_raw_ref json::operator[](const std::string& key) {
        return json_raw_ref(new json_ref_holder(impl->data[key]));
    }

    bool json::contains(const std::string& key) {
        return impl->data.contains(key);
    }

    json json::array(std::initializer_list<json_ref> init) {
        std::unique_ptr<json_holder> result(new json_holder);
        result->data = nlohmann::json::array();
        for (auto& elt : init) {
            result->data.push_back((*elt).impl->data);
        }

        json j;
        j.impl = std::move(result);
        return j;
    }

    void json::push_back(json&& val) {
        impl->data.push_back(std::move(val.impl->data));
    }
    void json::push_back(const json& val) {
        impl->data.push_back(val.impl->data);
    }

    json_raw_ref::json_raw_ref(json_ref_holder* ref_) : ref(ref_) {}
    json_raw_ref::json_raw_ref(const json_raw_ref& other)
        : ref(new json_ref_holder(other.ref->data)) {}
    json_raw_ref::~json_raw_ref() = default;

    std::string json_raw_ref::get_string() const noexcept {
        return ref->data.get<std::string>();
    }
    bool json_raw_ref::get_bool() const noexcept {
        return ref->data.get<bool>();
    }
    uint64_t json_raw_ref::get_uint64_t() const noexcept {
        return ref->data.get<uint64_t>();
    }
    int64_t json_raw_ref::get_int64_t() const noexcept {
        return ref->data.get<int64_t>();
    }
    double json_raw_ref::get_double() const noexcept {
        return ref->data.get<double>();
    }

    json_raw_ref& json_raw_ref::operator=(const size_t& other) {
        ref->data = other;
        return *this;
    }
    json_raw_ref& json_raw_ref::operator=(const double& other) {
        ref->data = other;
        return *this;
    }

    json_raw_ref json_raw_ref::operator[](size_t index) const {
        return json_raw_ref(new json_ref_holder(ref->data[index]));
    }
    std::vector<json_raw_ref> json_raw_ref::iterate() const {
        std::vector<json_raw_ref> result;
        for (auto& elt : ref->data) {
            result.push_back(json_raw_ref(new json_ref_holder(elt)));
        }
        return result;
    }

} // namespace hegel::internal::json
