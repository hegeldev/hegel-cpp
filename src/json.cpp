#include <hegel/json.h>
#include <nlohmann/json.hpp>
#include "json_impl.h"

using hegel::internal::json::ImplUtil;

namespace hegel::internal::json {

json::json(const json& init) : impl(new json_holder(*init.impl)) {}
json::json(json&& init) noexcept : impl(std::move(init.impl)) {}

json::json(initializer_list_t init) { 
    // check if each element is an array with two elements whose first
    // element is a string
    bool is_an_object = std::all_of(init.begin(), init.end(),
                                    [](const json_ref& my_ref)
    {
        const nlohmann::json& element_ref = (*my_ref).impl->data;

        // The cast is to ensure op[size_type] is called, bearing in mind size_type may not be int;
        // (many string types can be constructed from 0 via its null-pointer guise, so we get a
        // broken call to op[key_type], the wrong semantics and a 4804 warning on Windows)
        return element_ref.is_array() && element_ref.size() == 2 && element_ref[static_cast<size_t>(0)].is_string();
    });

    std::unique_ptr<json_holder> impl(new json_holder);
    if (is_an_object) {
        for (const json_ref& ref: init) {
            const nlohmann::json& elt = (*ref).impl->data;
            auto& key = elt[0];
            auto& value = elt[1];
            (impl->data)[key] = value;
        }
    } else {
        *impl = std::move(*json::array(init).impl);
    }
    this->impl = std::move(impl);
}

json::json(const char* init): impl(new json_holder(init)) {}
json::json(const int32_t init) : impl(new json_holder(init)) {}
json::json(const int64_t init) : impl(new json_holder(init)) {}
json::json(const uint32_t init) : impl(new json_holder(init)) {}
json::json(const uint64_t init) : impl(new json_holder(init)) {}
json::json(const bool init) : impl(new json_holder(init)) {}
json::json(const double init) : impl(new json_holder(init)) {}
json::json(const std::string& init) : impl(new json_holder(init)) {}
json::json(std::nullptr_t init) : impl(new json_holder(init)) {}
json::~json() = default;

json_raw_ref json::operator[](const std::string& key) {
    return json_raw_ref(new json_ref_holder(impl->data[key]));
}

json& json::operator=(json other) noexcept { 
    if (this != &other) {
        impl->operator=(*other.impl);
    }
    return *this;
}

std::string json::value(const std::string& key, const std::string& default_value) { return impl->data.value(key, default_value); }
uint32_t json::value(const std::string& key, const uint32_t& default_value) { return impl->data.value(key, default_value); }

bool json::contains(const std::string& key) { return impl->data.contains(key); }

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

void json::push_back(json&& val) { impl->data.push_back(std::move(val.impl->data)); }
void json::push_back(const json& val) { impl->data.push_back(val.impl->data); }
void json::push_back(const std::string& val) { impl->data.push_back(val); }

std::string json::dump() const { return impl->data.dump(); }

std::vector<unsigned char>& json::get_binary() { return impl->data.get_binary(); }

json json::parse(const char* arg) { 
    nlohmann::json result = nlohmann::json::parse(arg);
    return ImplUtil::create(result);
}



json_raw_ref::json_raw_ref(json_ref_holder* ref_) : ref(ref_) {}
json_raw_ref::json_raw_ref(const json_raw_ref& other) : ref(new json_ref_holder(other.ref->data)) {}
json_raw_ref::~json_raw_ref() = default;

std::string json_raw_ref::get_string() const noexcept { return ref->data.get<std::string>(); }
bool json_raw_ref::get_bool() const noexcept { return ref->data.get<bool>(); }
uint32_t json_raw_ref::get_uint32_t() const noexcept { return ref->data.get<uint32_t>(); }
uint64_t json_raw_ref::get_uint64_t() const noexcept { return ref->data.get<uint64_t>(); }
int64_t json_raw_ref::get_int64_t() const noexcept { return ref->data.get<int64_t>(); }
double json_raw_ref::get_double() const noexcept { return ref->data.get<double>(); }

size_t json_raw_ref::size() const noexcept { return ref->data.size(); }

json_raw_ref& json_raw_ref::operator=(const size_t& other) { ref->data=other; return *this; }
json_raw_ref& json_raw_ref::operator=(const std::nullptr_t& other) { ref->data=other; return *this; }
json_raw_ref& json_raw_ref::operator=(const double& other)  { ref->data=other; return *this; }

bool json_raw_ref::is_string() const noexcept { return ref->data.is_string(); }
bool json_raw_ref::is_null() const noexcept { return ref->data.is_null(); }
bool json_raw_ref::is_boolean() const noexcept { return ref->data.is_boolean(); }
bool json_raw_ref::is_number() const noexcept { return ref->data.is_number(); }
bool json_raw_ref::is_number_integer() const noexcept { return ref->data.is_number_integer(); }
bool json_raw_ref::is_number_unsigned() const noexcept { return ref->data.is_number_unsigned(); }
bool json_raw_ref::is_array() const noexcept { return ref->data.is_array(); }
bool json_raw_ref::is_object() const noexcept { return ref->data.is_object(); }

json_raw_ref json_raw_ref::operator[](size_t index) const { return json_raw_ref(new json_ref_holder(ref->data[index])); }
std::vector<json_raw_ref> json_raw_ref::iterate() const {
    std::vector<json_raw_ref> result;
    for (auto& elt : ref->data) {
        result.push_back(json_raw_ref(new json_ref_holder(elt)));
    }
    return result;
}

std::vector<std::pair<std::string, json_raw_ref>> json_raw_ref::items() const {
    std::vector<std::pair<std::string, json_raw_ref>> result;
    for (auto& [key, val] : ref->data.items()) {
        result.push_back(std::make_pair(key, json_raw_ref(new json_ref_holder(val))));
    }
    return result;
}

std::optional<json_raw_ref> json_raw_ref::find(const std::string& key) const {
    auto it = ref->data.find(key);
    return (it == ref->data.end()) ? std::nullopt : std::optional(json_raw_ref(new json_ref_holder(*it)));
}

}