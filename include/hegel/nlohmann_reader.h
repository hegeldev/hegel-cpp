#pragma once

/**
 * @file nlohmann_reader.h
 * @brief reflect-cpp Reader for nlohmann::json (avoids JSON text round-trip)
 * @cond INTERNAL
 */

#include <nlohmann/json.hpp>
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
        struct InputArray {
            nlohmann::json* val_;
        };
        struct InputObject {
            nlohmann::json* val_;
        };
        struct InputVar {
            nlohmann::json* val_;
        };

        using InputArrayType = InputArray;
        using InputObjectType = InputObject;
        using InputVarType = InputVar;

        template <class T> static constexpr bool has_custom_constructor = false;

        rfl::Result<InputVarType>
        get_field_from_array(const size_t _idx,
                             const InputArrayType& _arr) const noexcept {
            if (_idx >= _arr.val_->size()) {
                return rfl::error("Index out of range.");
            }
            return InputVarType{&(*_arr.val_)[_idx]};
        }

        rfl::Result<InputVarType>
        get_field_from_object(const std::string& _name,
                              const InputObjectType& _obj) const noexcept {
            auto it = _obj.val_->find(_name);
            if (it == _obj.val_->end()) {
                return rfl::error("Field name '" + _name + "' not found.");
            }
            return InputVarType{&(*it)};
        }

        bool is_empty(const InputVarType& _var) const noexcept {
            return _var.val_->is_null();
        }

        template <class T>
        rfl::Result<T> to_basic_type(const InputVarType& _var) const noexcept {
            if constexpr (std::is_same<std::remove_cvref_t<T>, std::string>()) {
                if (!_var.val_->is_string()) {
                    return rfl::error("Could not cast to string.");
                }
                return _var.val_->get<std::string>();

            } else if constexpr (std::is_same<std::remove_cvref_t<T>,
                                              rfl::Bytestring>() ||
                                 std::is_same<std::remove_cvref_t<T>,
                                              rfl::Vectorstring>()) {
                return rfl::error("Byte/vector strings not supported.");

            } else if constexpr (std::is_same<std::remove_cvref_t<T>, bool>()) {
                if (!_var.val_->is_boolean()) {
                    return rfl::error("Could not cast to boolean.");
                }
                return _var.val_->get<bool>();

            } else if constexpr (std::is_floating_point<
                                     std::remove_cvref_t<T>>()) {
                if (!_var.val_->is_number()) {
                    return rfl::error("Could not cast to double.");
                }
                return static_cast<T>(_var.val_->get<double>());

            } else if constexpr (std::is_integral<std::remove_cvref_t<T>>()) {
                if (_var.val_->is_number_integer()) {
                    return static_cast<T>(_var.val_->get<int64_t>());
                }
                if (_var.val_->is_number_unsigned()) {
                    return static_cast<T>(_var.val_->get<uint64_t>());
                }
                return rfl::error("Could not cast to integer.");

            } else {
                static_assert(rfl::always_false_v<T>, "Unsupported type.");
            }
        }

        rfl::Result<InputArrayType>
        to_array(const InputVarType& _var) const noexcept {
            if (!_var.val_->is_array()) {
                return rfl::error("Could not cast to an array.");
            }
            return InputArrayType{_var.val_};
        }

        rfl::Result<InputObjectType>
        to_object(const InputVarType& _var) const noexcept {
            if (!_var.val_->is_object()) {
                return rfl::error("Could not cast to an object.");
            }
            return InputObjectType{_var.val_};
        }

        template <class ArrayReader>
        std::optional<rfl::Error>
        read_array(const ArrayReader& _array_reader,
                   const InputArrayType& _arr) const noexcept {
            for (auto& val : *_arr.val_) {
                const auto err = _array_reader.read(InputVarType{&val});
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
            for (auto& [key, val] : _obj.val_->items()) {
                _object_reader.read(key, InputVarType{&val});
            }
            return std::nullopt;
        }

        template <class T>
        rfl::Result<T>
        use_custom_constructor(const InputVarType&) const noexcept {
            return rfl::error("Custom constructors not supported.");
        }
    };

    /// Deserialize a nlohmann::json value into type T using reflect-cpp.
    template <class T> rfl::Result<T> read_nlohmann(nlohmann::json& val) {
        auto r = NlohmannReader();
        return rfl::parsing::Parser<
            NlohmannReader, rfl::json::Writer, T,
            rfl::Processors<>>::read(r, NlohmannReader::InputVarType{&val});
    }

} // namespace hegel::internal

/// @endcond
