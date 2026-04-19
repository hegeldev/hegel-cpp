#pragma once

/**
 * @cond INTERNAL
 */

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace hegel::internal {
    class NlohmannReader;
}

namespace hegel::internal::json {
    class json;
    class json_ref;
    class ImplUtil;
    struct json_ref_holder;
    struct json_holder;

    class json_raw_ref {
        std::unique_ptr<json_ref_holder> ref;
        friend class ImplUtil;

      public:
        json_raw_ref(json_ref_holder* ref_);
        json_raw_ref(const json_raw_ref& other);
        ~json_raw_ref();

        std::string get_string() const noexcept;
        bool get_bool() const noexcept;
        uint32_t get_uint32_t() const noexcept;
        uint64_t get_uint64_t() const noexcept;
        int64_t get_int64_t() const noexcept;
        double get_double() const noexcept;

        size_t size() const noexcept;

        bool is_string() const noexcept;
        bool is_null() const noexcept;
        bool is_boolean() const noexcept;
        bool is_number() const noexcept;
        bool is_number_integer() const noexcept;
        bool is_number_unsigned() const noexcept;
        bool is_array() const noexcept;
        bool is_object() const noexcept;

        json_raw_ref& operator=(const size_t& other);
        json_raw_ref& operator=(const double& other);
        json_raw_ref& operator=(const std::nullptr_t& other);
        json_raw_ref& operator=(bool other);
        json_raw_ref& operator=(const std::string& other);
        json_raw_ref& operator=(const json& other);
#ifdef __APPLE__
        json_raw_ref& operator=(const uint64_t& other);
#endif

        json_raw_ref operator[](size_t index) const;

        std::vector<json_raw_ref> iterate() const;
        std::vector<std::pair<std::string, json_raw_ref>> items() const;
        std::optional<json_raw_ref> find(const std::string& key) const;
    };

    class json {
        using initializer_list_t = std::initializer_list<json_ref>;

      public:
        json(const json& init);

        json(json&& init) noexcept;

        json(initializer_list_t init);

        json(const char* init);

        json(const int32_t init);
        json(const int64_t init);
        json(const uint32_t init);
        json(const uint64_t init);
#ifdef __APPLE__
        json(const unsigned long init);
#endif
        json(const bool init);
        json(const double init);
        json(const std::string& init);
        json(std::nullptr_t init = nullptr);
        ~json();

        json_raw_ref operator[](const std::string& key);

        json& operator=(json other) noexcept;

        std::string value(const std::string& key,
                          const std::string& default_value);
        uint32_t value(const std::string& key, const uint32_t& default_value);

        bool contains(const std::string& key);

        static json array(initializer_list_t init = {});
        void push_back(json&& val);
        void push_back(const json& val);
        void push_back(const std::string& val);

        std::string dump() const;

        std::vector<unsigned char>& get_binary();

        static json parse(const char* arg);

      private:
        std::unique_ptr<json_holder> impl;
        friend class ImplUtil;
    };

    class json_ref {
      public:
        json_ref(json&& value) : owned_value(std::move(value)) {}

        json_ref(const json& value) : value_ref(&value) {}

        json_ref(std::initializer_list<json_ref> init) : owned_value(init) {}

        template <class... Args,
                  std::enable_if_t<std::is_constructible<json, Args...>::value,
                                   int> = 0>
        json_ref(Args&&... args) : owned_value(std::forward<Args>(args)...) {}

        // class should be movable only
        json_ref(json_ref&&) noexcept = default;
        json_ref(const json_ref&) = delete;
        json_ref& operator=(const json_ref&) = delete;
        json_ref& operator=(json_ref&&) = delete;
        ~json_ref() = default;

        json const& operator*() const {
            return value_ref ? *value_ref : owned_value;
        }

      private:
        mutable json owned_value = nullptr;
        json const* value_ref = nullptr;
    };
} // namespace hegel::internal::json
/// @endcond
