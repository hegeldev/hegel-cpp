#pragma once

/**
 * @cond INTERNAL
 */

#include "json.h"
#include <rfl/Bytestring.hpp>
#include <rfl/Processors.hpp>
#include <rfl/Result.hpp>
#include <rfl/Vectorstring.hpp>
#include <rfl/always_false.hpp>
#include <rfl/json/Writer.hpp>
#include <rfl/parsing/Parser.hpp>

namespace hegel::internal {

    /// A reflect-cpp Reader that reads directly from nlohmann::json pointers.
    /// This avoids a JSON text round-trip that would lose NaN/infinity.
    class NlohmannReader {
      public:
        using InputArrayType = hegel::internal::json::json_raw_ref;
        using InputObjectType = hegel::internal::json::json_raw_ref;
        using InputVarType = hegel::internal::json::json_raw_ref;

        template <class T> static constexpr bool has_custom_constructor = false;

        rfl::Result<InputVarType>
        get_field_from_array(const size_t _idx,
                             const InputArrayType& _arr) const noexcept {
            if (_idx >= _arr.size()) {
                return rfl::error("Index out of range.");
            }
            return InputVarType{_arr[_idx]};
        }

        rfl::Result<InputVarType>
        get_field_from_object(const std::string& _name,
                              const InputObjectType& _obj) const noexcept {
            auto it = _obj.find(_name);
            if (!it.has_value()) {
                return rfl::error("Field name '" + _name + "' not found.");
            }
            return InputVarType{it.value()};
        }

        bool is_empty(const InputVarType& _var) const noexcept {
            return _var.is_null();
        }

        template <class T>
        rfl::Result<T> to_basic_type(const InputVarType& _var) const noexcept {
            if constexpr (std::is_same<std::remove_cvref_t<T>, std::string>()) {
                if (!_var.is_string()) {
                    return rfl::error("Could not cast to string.");
                }
                return _var.get_string();

            } else if constexpr (std::is_same<std::remove_cvref_t<T>,
                                              rfl::Bytestring>() ||
                                 std::is_same<std::remove_cvref_t<T>,
                                              rfl::Vectorstring>()) {
                return rfl::error("Byte/vector strings not supported.");

            } else if constexpr (std::is_same<std::remove_cvref_t<T>, bool>()) {
                if (!_var.is_boolean()) {
                    return rfl::error("Could not cast to boolean.");
                }
                return _var.get_bool();

            } else if constexpr (std::is_floating_point<
                                     std::remove_cvref_t<T>>()) {
                if (!_var.is_number()) {
                    return rfl::error("Could not cast to double.");
                }
                return static_cast<T>(_var.get_double());

            } else if constexpr (std::is_integral<std::remove_cvref_t<T>>()) {
                if (_var.is_number_integer()) {
                    return static_cast<T>(_var.get_int64_t());
                }
                if (_var.is_number_unsigned()) {
                    return static_cast<T>(_var.get_uint64_t());
                }
                return rfl::error("Could not cast to integer.");

            } else {
                static_assert(rfl::always_false_v<T>, "Unsupported type.");
            }
        }

        rfl::Result<InputArrayType>
        to_array(const InputVarType& _var) const noexcept {
            if (!_var.is_array()) {
                return rfl::error("Could not cast to an array.");
            }
            return InputArrayType(_var);
        }

        rfl::Result<InputObjectType>
        to_object(const InputVarType& _var) const noexcept {
            if (!_var.is_object()) {
                return rfl::error("Could not cast to an object.");
            }
            return InputObjectType(_var);
        }

        template <class ArrayReader>
        std::optional<rfl::Error>
        read_array(const ArrayReader& _array_reader,
                   const InputArrayType& _arr) const noexcept {
            auto to_iterate = _arr.iterate();
            for (auto& val : to_iterate) {
                const auto err = _array_reader.read(InputVarType{val});
                if (err) {
                    return err;
                }
            }
            return std::nullopt;
        }

        template <class ObjectReader>
        std::optional<rfl::Error>
        read_object(const ObjectReader& _object_reader,
                    const InputObjectType& _obj) const noexcept {
            auto items = _obj.items();
            for (auto& [key, val] : items) {
                _object_reader.read(key, InputVarType{val});
            }
            return std::nullopt;
        }

        template <class T>
        rfl::Result<T>
        use_custom_constructor(const InputVarType&) const noexcept {
            return rfl::error("Custom constructors not supported.");
        }
    };

    /// Deserialize a hegel::internal::json::json value into type T using
    /// reflect-cpp.
    template <class T>
    rfl::Result<T>
    read_nlohmann(const hegel::internal::json::json_raw_ref& val) {
        auto r = NlohmannReader();
        return rfl::parsing::Parser<
            NlohmannReader, rfl::json::Writer, T,
            rfl::Processors<>>::read(r, NlohmannReader::InputVarType(val));
    }

} // namespace hegel::internal

/// @endcond
