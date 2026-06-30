#pragma once

/**
 * @cond INTERNAL
 */

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

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
        uint64_t get_uint64_t() const noexcept;
        int64_t get_int64_t() const noexcept;
        double get_double() const noexcept;

        json_raw_ref& operator=(const size_t& other);
        json_raw_ref& operator=(const double& other);

        json_raw_ref operator[](size_t index) const;

        std::vector<json_raw_ref> iterate() const;
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
        json(const std::string& init);
        json(std::nullptr_t init = nullptr);
        ~json();

        json_raw_ref operator[](const std::string& key);

        bool contains(const std::string& key);

        static json array(initializer_list_t init = {});
        void push_back(json&& val);
        void push_back(const json& val);

      private:
        std::unique_ptr<json_holder> impl;
        friend class ImplUtil;
    };

    class json_ref {
      public:
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

        json const& operator*() const { return owned_value; }

      private:
        json owned_value = nullptr;
    };
} // namespace hegel::internal::json
/// @endcond
