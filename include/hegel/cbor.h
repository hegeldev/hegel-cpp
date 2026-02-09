#pragma once

/**
 * @file cbor.h
 * @brief CBOR value type and helpers for Hegel SDK
 *
 * Provides a Value type and helper functions for building CBOR values.
 * Used internally for schema construction and protocol communication.
 */

#include <cstdint>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace hegel::cbor {

/// CBOR value type
using Value = nlohmann::json;

// =============================================================================
// CBOR Value Constructors
// =============================================================================

/// Create a CBOR null value
inline Value null() { return Value(nullptr); }

/// Create a CBOR boolean value
inline Value boolean(bool b) { return Value(b); }

/// Create a CBOR integer value
template <typename T>
requires std::is_integral_v<T>
inline Value integer(T n) { return Value(n); }

/// Create a CBOR float value
template <typename T>
requires std::is_floating_point_v<T>
inline Value floating(T f) { return Value(f); }

/// Create a CBOR text string value
inline Value text(const std::string& s) { return Value(s); }
inline Value text(const char* s) { return Value(s); }

/// Create a CBOR array from values
inline Value array(std::initializer_list<Value> items) { return Value(items); }

/// Create an empty CBOR array
inline Value array() { return Value::array(); }

/// Create a CBOR map from key-value pairs
inline Value map(std::initializer_list<std::pair<std::string, Value>> items) {
    Value m = Value::object();
    for (const auto& [k, v] : items) {
        m[k] = v;
    }
    return m;
}

/// Create an empty CBOR map
inline Value map() { return Value::object(); }

// =============================================================================
// CBOR Map Accessors
// =============================================================================

/// Get a value from a CBOR map by key, returns nullopt if not found
inline std::optional<Value> map_get(const Value& m, const std::string& key) {
    if (m.is_object() && m.contains(key)) {
        return m[key];
    }
    return std::nullopt;
}

/// Insert or update a key in a CBOR map
inline void map_insert(Value& m, const std::string& key, Value val) {
    if (m.is_object()) {
        m[key] = std::move(val);
    }
}

// =============================================================================
// CBOR Value Extractors
// =============================================================================

/// Extract text string from CBOR value
inline std::optional<std::string> as_text(const Value& v) {
    if (v.is_string()) {
        return v.get<std::string>();
    }
    return std::nullopt;
}

/// Extract text string from optional CBOR value
inline std::optional<std::string> as_text(const std::optional<Value>& v) {
    if (v) {
        return as_text(*v);
    }
    return std::nullopt;
}

/// Extract integer from CBOR value
inline std::optional<int64_t> as_int(const Value& v) {
    if (v.is_number_integer()) {
        return v.get<int64_t>();
    }
    return std::nullopt;
}

/// Extract integer from optional CBOR value
inline std::optional<int64_t> as_int(const std::optional<Value>& v) {
    if (v) {
        return as_int(*v);
    }
    return std::nullopt;
}

/// Extract unsigned integer from CBOR value
inline std::optional<uint64_t> as_uint(const Value& v) {
    if (v.is_number_unsigned()) {
        return v.get<uint64_t>();
    }
    if (v.is_number_integer()) {
        auto n = v.get<int64_t>();
        if (n >= 0) {
            return static_cast<uint64_t>(n);
        }
    }
    return std::nullopt;
}

/// Extract unsigned integer from optional CBOR value
inline std::optional<uint64_t> as_uint(const std::optional<Value>& v) {
    if (v) {
        return as_uint(*v);
    }
    return std::nullopt;
}

/// Extract float from CBOR value
inline std::optional<double> as_float(const Value& v) {
    if (v.is_number()) {
        return v.get<double>();
    }
    return std::nullopt;
}

/// Extract float from optional CBOR value
inline std::optional<double> as_float(const std::optional<Value>& v) {
    if (v) {
        return as_float(*v);
    }
    return std::nullopt;
}

/// Extract boolean from CBOR value
inline std::optional<bool> as_bool(const Value& v) {
    if (v.is_boolean()) {
        return v.get<bool>();
    }
    return std::nullopt;
}

/// Extract boolean from optional CBOR value
inline std::optional<bool> as_bool(const std::optional<Value>& v) {
    if (v) {
        return as_bool(*v);
    }
    return std::nullopt;
}

// =============================================================================
// CBOR Serialization
// =============================================================================

/// Encode a CBOR value to bytes
inline std::vector<uint8_t> encode(const Value& v) { return Value::to_cbor(v); }

/// Decode CBOR bytes to a value
inline Value decode(const std::vector<uint8_t>& bytes) {
    return Value::from_cbor(bytes, true, true,
                            Value::cbor_tag_handler_t::ignore);
}

/// Decode CBOR bytes to a value (pointer + length version)
inline Value decode(const uint8_t* data, size_t len) {
    return Value::from_cbor(data, data + len, true, true,
                            Value::cbor_tag_handler_t::ignore);
}

} // namespace hegel::cbor

// Backward compatibility alias for internal code
namespace hegel::impl::cbor {
using namespace hegel::cbor;
}
