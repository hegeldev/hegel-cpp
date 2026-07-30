#pragma once

/**
 * @cond INTERNAL
 */

// Renders drawn values as C++ expressions for draw-output lines. Types the
// library cannot render as a compilable expression fall back to the text of
// their stream operator, then to an "<unprintable Type>" placeholder.

#include "config.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if HEGEL_HAS_REFLECTION
#include <rfl.hpp>
#endif

namespace hegel::internal {

    template <typename T> std::string cpp_type_name();
    template <typename T> std::string repr(const T& value);
    template <typename T> constexpr bool renderable();

    // A rendered value, kept as a tree so a difference of two values can
    // descend to the part that changed.
    struct ReprNode {
        // What names this node inside its parent: ".x = " for a struct
        // field, empty otherwise.
        std::string label;
        // The value's own rendering, without the label.
        std::string text;
        // The opening of a composite value, up to and including its brace:
        // "std::vector<int>{". Empty for a value with no parts.
        std::string prefix;
        // The parts the value is made of, empty for a value with none.
        std::vector<ReprNode> parts;

        // How the node reads inside its parent, which is what decides
        // whether two nodes are the same part.
        std::string full() const { return label + text; }
        bool composite() const { return !prefix.empty(); }
    };

    template <typename T> ReprNode repr_node(const T& value);

    // Best-effort type name sliced from the compiler's pretty function
    // string. Used for placeholder output and for types with no clean
    // spelling of their own.
    template <typename T> constexpr std::string_view compiler_type_name() {
#if defined(__clang__)
        std::string_view p = __PRETTY_FUNCTION__;
        auto start = p.find("T = ");
        auto end = p.rfind(']');
        if (start == std::string_view::npos || end == std::string_view::npos) {
            return "?"; // GCOVR_EXCL_LINE - guards a compiler format change
        }
        start += 4;
        return p.substr(start, end - start);
#elif defined(__GNUC__)
        std::string_view p = __PRETTY_FUNCTION__;
        auto start = p.find("T = ");
        if (start == std::string_view::npos) {
            return "?";
        }
        start += 4;
        auto end = p.find(';', start);
        if (end == std::string_view::npos) {
            end = p.rfind(']');
        }
        if (end == std::string_view::npos) {
            return "?";
        }
        return p.substr(start, end - start);
#elif defined(_MSC_VER)
        std::string_view p = __FUNCSIG__;
        constexpr std::string_view marker = "compiler_type_name<";
        auto start = p.find(marker);
        auto end = p.rfind(">(");
        if (start == std::string_view::npos || end == std::string_view::npos) {
            return "?";
        }
        start += marker.size();
        return p.substr(start, end - start);
#else
        return "?";
#endif
    }

    namespace repr_detail {

        template <typename T> struct is_vector : std::false_type {};
        template <typename T>
        struct is_vector<std::vector<T>> : std::true_type {};

        template <typename T> struct is_set : std::false_type {};
        template <typename T> struct is_set<std::set<T>> : std::true_type {};

        template <typename T> struct is_map : std::false_type {};
        template <typename K, typename V>
        struct is_map<std::map<K, V>> : std::true_type {};

        template <typename T> struct is_optional : std::false_type {};
        template <typename T>
        struct is_optional<std::optional<T>> : std::true_type {};

        template <typename T> struct is_pair : std::false_type {};
        template <typename A, typename B>
        struct is_pair<std::pair<A, B>> : std::true_type {};

        template <typename T> struct is_tuple : std::false_type {};
        template <typename... Ts>
        struct is_tuple<std::tuple<Ts...>> : std::true_type {};

        template <typename T> struct is_variant : std::false_type {};
        template <typename... Ts>
        struct is_variant<std::variant<Ts...>> : std::true_type {};

        template <typename T> struct is_std_array : std::false_type {};
        template <typename E, std::size_t N>
        struct is_std_array<std::array<E, N>> : std::true_type {};

        template <typename T> struct array_traits;
        template <typename E, std::size_t N>
        struct array_traits<std::array<E, N>> {
            using element_type = E;
            static constexpr std::size_t size = N;
        };

        template <typename T, typename = void>
        struct has_ostream_operator : std::false_type {};
        template <typename T>
        struct has_ostream_operator<
            T, std::void_t<decltype(std::declval<std::ostream&>()
                                    << std::declval<const T&>())>>
            : std::true_type {};

#if HEGEL_HAS_REFLECTION
        template <typename T, typename = void>
        struct is_reflectable_impl : std::false_type {};
        template <typename T>
        struct is_reflectable_impl<
            T, std::void_t<decltype(rfl::to_view(std::declval<T&>()))>>
            : std::true_type {};

        // Checks if type D has a base class at compile time
        template <typename D> struct base_probe {
            base_probe(std::size_t);
            template <typename B>
                requires(std::is_base_of_v<std::remove_cvref_t<B>, D> &&
                         !std::is_same_v<std::remove_cvref_t<B>, D>)
            constexpr operator B&() const noexcept;
        };

        // A struct that inherits is an aggregate that rfl::to_view still cannot
        // view. Its structured binding needs every member in one class.
        template <typename T>
        inline constexpr bool has_base_class = requires {
            std::remove_cv_t<T>{base_probe<std::remove_cv_t<T>>(0)};
        };

        template <typename T>
        struct is_reflectable
            : std::conjunction<
                  std::is_aggregate<T>,
                  std::negation<std::bool_constant<has_base_class<T>>>,
                  is_reflectable_impl<T>> {};
#endif

        template <typename... Ts> std::string type_name_list() {
            if constexpr (sizeof...(Ts) == 0) {
                return "";
            } else {
                std::string names[] = {cpp_type_name<Ts>()...};
                std::string out;
                for (std::size_t i = 0; i < sizeof...(Ts); ++i) {
                    if (i != 0) {
                        out += ", ";
                    }
                    out += names[i];
                }
                return out;
            }
        }

        template <typename T> struct template_args;
        template <typename... Ts> struct template_args<std::tuple<Ts...>> {
            static std::string names() { return type_name_list<Ts...>(); }
        };
        template <typename... Ts> struct template_args<std::variant<Ts...>> {
            static std::string names() { return type_name_list<Ts...>(); }
        };

        // Appends one byte as it must appear inside a char or string
        // literal: named escapes, printable ASCII verbatim, all other
        // bytes as a fixed three-digit octal escape (a following literal
        // digit can never extend a three-digit octal escape).
        inline void append_escaped(std::string& out, unsigned char c,
                                   bool in_string) {
            switch (c) {
            case '\n':
                out += "\\n";
                return;
            case '\t':
                out += "\\t";
                return;
            case '\r':
                out += "\\r";
                return;
            case '\\':
                out += "\\\\";
                return;
            case '"':
                if (in_string) {
                    out += "\\\"";
                    return;
                }
                break;
            case '\'':
                if (!in_string) {
                    out += "\\'";
                    return;
                }
                break;
            default:
                break;
            }
            if (c >= 0x20 && c < 0x7f) {
                out += static_cast<char>(c);
                return;
            }
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\%03o", c);
            out += buf;
        }

    } // namespace repr_detail

    namespace repr_detail {

        // The parts of a compound type, for the branches of renderable()
        // that hold more than one type.
        template <typename T> struct parts_renderable;
        template <typename... Ts> struct parts_renderable<std::tuple<Ts...>> {
            static constexpr bool value = (renderable<Ts>() && ...);
        };
        template <typename... Ts> struct parts_renderable<std::variant<Ts...>> {
            static constexpr bool value = (renderable<Ts>() && ...);
        };

#if HEGEL_HAS_REFLECTION
        // A view holds one field per member, and the field's Type is a
        // pointer to that member.
        template <typename View> struct fields_renderable;
        template <template <typename...> class Tuple, typename... Fields>
        struct fields_renderable<Tuple<Fields...>> {
            static constexpr bool value =
                (renderable<std::remove_cv_t<
                     std::remove_pointer_t<typename Fields::Type>>>() &&
                 ...);
        };
#endif

    } // namespace repr_detail

    template <typename T> constexpr bool renderable() {
        if constexpr (std::is_integral_v<T> || std::is_enum_v<T> ||
                      std::is_floating_point_v<T> ||
                      std::is_same_v<T, std::string> ||
                      std::is_same_v<T, std::monostate>) {
            return true;
        } else if constexpr (repr_detail::is_pair<T>::value) {
            return renderable<typename T::first_type>() &&
                   renderable<typename T::second_type>();
        } else if constexpr (repr_detail::is_tuple<T>::value ||
                             repr_detail::is_variant<T>::value) {
            return repr_detail::parts_renderable<T>::value;
        } else if constexpr (repr_detail::is_optional<T>::value ||
                             repr_detail::is_vector<T>::value ||
                             repr_detail::is_set<T>::value ||
                             repr_detail::is_std_array<T>::value) {
            return renderable<typename T::value_type>();
        } else if constexpr (repr_detail::is_map<T>::value) {
            return renderable<typename T::key_type>() &&
                   renderable<typename T::mapped_type>();
        }
#if HEGEL_HAS_REFLECTION
        else if constexpr (repr_detail::is_reflectable<T>::value) {
            return repr_detail::fields_renderable<decltype(rfl::to_view(
                std::declval<T&>()))>::value;
        }
#endif
        else {
            return repr_detail::has_ostream_operator<T>::value;
        }
    }

    template <typename T>
    inline constexpr bool is_renderable_v = renderable<T>();

    // C++ source spelling of T, for use inside rendered expressions.
    template <typename T> std::string cpp_type_name() {
        if constexpr (std::is_same_v<T, bool>) {
            return "bool";
        } else if constexpr (std::is_same_v<T, char>) {
            return "char";
        } else if constexpr (std::is_same_v<T, signed char>) {
            return "signed char";
        } else if constexpr (std::is_same_v<T, unsigned char>) {
            return "unsigned char";
        } else if constexpr (std::is_same_v<T, short>) {
            return "short";
        } else if constexpr (std::is_same_v<T, unsigned short>) {
            return "unsigned short";
        } else if constexpr (std::is_same_v<T, int>) {
            return "int";
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            return "unsigned int";
        } else if constexpr (std::is_same_v<T, long>) {
            return "long";
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            return "unsigned long";
        } else if constexpr (std::is_same_v<T, long long>) {
            return "long long";
        } else if constexpr (std::is_same_v<T, unsigned long long>) {
            return "unsigned long long";
        } else if constexpr (std::is_same_v<T, float>) {
            return "float";
        } else if constexpr (std::is_same_v<T, double>) {
            return "double";
        } else if constexpr (std::is_same_v<T, long double>) {
            return "long double";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "std::string";
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            return "std::monostate";
        } else if constexpr (repr_detail::is_vector<T>::value) {
            return "std::vector<" + cpp_type_name<typename T::value_type>() +
                   ">";
        } else if constexpr (repr_detail::is_set<T>::value) {
            return "std::set<" + cpp_type_name<typename T::value_type>() + ">";
        } else if constexpr (repr_detail::is_map<T>::value) {
            return "std::map<" + cpp_type_name<typename T::key_type>() + ", " +
                   cpp_type_name<typename T::mapped_type>() + ">";
        } else if constexpr (repr_detail::is_optional<T>::value) {
            return "std::optional<" + cpp_type_name<typename T::value_type>() +
                   ">";
        } else if constexpr (repr_detail::is_pair<T>::value) {
            return "std::pair<" + cpp_type_name<typename T::first_type>() +
                   ", " + cpp_type_name<typename T::second_type>() + ">";
        } else if constexpr (repr_detail::is_tuple<T>::value) {
            return "std::tuple<" + repr_detail::template_args<T>::names() + ">";
        } else if constexpr (repr_detail::is_variant<T>::value) {
            return "std::variant<" + repr_detail::template_args<T>::names() +
                   ">";
        } else if constexpr (repr_detail::is_std_array<T>::value) {
            using traits = repr_detail::array_traits<T>;
            return "std::array<" +
                   cpp_type_name<typename traits::element_type>() + ", " +
                   std::to_string(traits::size) + ">";
        } else {
            return std::string(compiler_type_name<T>());
        }
    }

    // Renders a value as one C++ expression, or as a pseudo-expression for
    // types with no renderable form.
    template <typename T> std::string repr(const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, char>) {
            std::string out = "'";
            repr_detail::append_escaped(out, static_cast<unsigned char>(value),
                                        /*in_string=*/false);
            out += "'";
            return out;
        } else if constexpr (std::is_same_v<T, signed char> ||
                             std::is_same_v<T, unsigned char>) {
            return "static_cast<" + cpp_type_name<T>() + ">(" +
                   std::to_string(static_cast<int>(value)) + ")";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            return std::to_string(value) + "u";
        } else if constexpr (std::is_same_v<T, long>) {
            return std::to_string(value) + "l";
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            return std::to_string(value) + "ul";
        } else if constexpr (std::is_same_v<T, long long>) {
            return std::to_string(value) + "ll";
        } else if constexpr (std::is_same_v<T, unsigned long long>) {
            return std::to_string(value) + "ull";
        } else if constexpr (std::is_integral_v<T>) {
            // Integral types with no literal suffix of their own.
            return "static_cast<" + cpp_type_name<T>() + ">(" +
                   std::to_string(value) + ")";
        } else if constexpr (std::is_enum_v<T>) {
            using underlying = std::underlying_type_t<T>;
            return "static_cast<" + cpp_type_name<T>() + ">(" +
                   repr(static_cast<underlying>(value)) + ")";
        } else if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(value)) {
                return "std::numeric_limits<" + cpp_type_name<T>() +
                       ">::quiet_NaN()";
            }
            if (std::isinf(value)) {
                std::string inf = "std::numeric_limits<" + cpp_type_name<T>() +
                                  ">::infinity()";
                return value < 0 ? "-" + inf : inf;
            }
            char buf[64];
            constexpr int digits = std::numeric_limits<T>::max_digits10;
            if constexpr (std::is_same_v<T, long double>) {
                std::snprintf(buf, sizeof(buf), "%.*Lg", digits, value);
            } else {
                std::snprintf(buf, sizeof(buf), "%.*g", digits,
                              static_cast<double>(value));
            }
            std::string out = buf;
            // keep integral-valued float rendering as float since %g drops
            // decimal when it isn't needed.
            if (out.find('.') == std::string::npos &&
                out.find('e') == std::string::npos &&
                out.find('E') == std::string::npos) {
                out += ".0";
            }
            if constexpr (std::is_same_v<T, float>) {
                out += "f";
            } else if constexpr (std::is_same_v<T, long double>) {
                out += "L";
            }
            return out;
        } else if constexpr (std::is_same_v<T, std::string>) {
            bool has_nul = value.find('\0') != std::string::npos;
            std::string out = "std::string(\"";
            for (char c : value) {
                repr_detail::append_escaped(out, static_cast<unsigned char>(c),
                                            /*in_string=*/true);
            }
            out += "\"";
            if (has_nul) {
                // The pointer constructor would stop at the first NUL.
                out += ", " + std::to_string(value.size());
            }
            out += ")";
            return out;
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            return "std::monostate{}";
        } else if constexpr (repr_detail::is_pair<T>::value) {
            return cpp_type_name<T>() + "{" + repr(value.first) + ", " +
                   repr(value.second) + "}";
        } else if constexpr (repr_detail::is_tuple<T>::value) {
            std::string out = cpp_type_name<T>() + "{";
            std::apply(
                [&out](const auto&... elems) {
                    std::size_t i = 0;
                    ((out +=
                      repr(elems) + (++i < sizeof...(elems) ? ", " : "")),
                     ...);
                },
                value);
            out += "}";
            return out;
        } else if constexpr (repr_detail::is_optional<T>::value) {
            std::string name = cpp_type_name<T>();
            return value.has_value() ? name + "{" + repr(*value) + "}"
                                     : name + "{}";
        } else if constexpr (repr_detail::is_variant<T>::value) {
            // in_place_index keeps variants with repeated alternative types
            // unambiguous.
            std::string out = cpp_type_name<T>() + "{std::in_place_index<" +
                              std::to_string(value.index()) + ">, ";
            std::visit([&out](const auto& v) { out += repr(v); }, value);
            out += "}";
            return out;
        } else if constexpr (repr_detail::is_vector<T>::value ||
                             repr_detail::is_set<T>::value ||
                             repr_detail::is_std_array<T>::value) {
            std::string out = cpp_type_name<T>() + "{";
            bool first = true;
            for (const auto& element : value) {
                if (!first) {
                    out += ", ";
                }
                first = false;
                out += repr(element);
            }
            out += "}";
            return out;
        } else if constexpr (repr_detail::is_map<T>::value) {
            std::string out = cpp_type_name<T>() + "{";
            bool first = true;
            for (const auto& entry : value) {
                if (!first) {
                    out += ", ";
                }
                first = false;
                out +=
                    "{" + repr(entry.first) + ", " + repr(entry.second) + "}";
            }
            out += "}";
            return out;
        }
#if HEGEL_HAS_REFLECTION
        else if constexpr (repr_detail::is_reflectable<T>::value) {
            std::string out = cpp_type_name<T>() + "{";
            auto view = rfl::to_view(const_cast<T&>(value));
            bool first = true;
            view.apply([&out, &first](const auto& field) {
                if (!first) {
                    out += ", ";
                }
                first = false;
                out += ".";
                out += field.name();
                out += " = ";
                out += repr(*field.value());
            });
            out += "}";
            return out;
        }
#endif
        else if constexpr (repr_detail::has_ostream_operator<T>::value) {
            std::ostringstream os;
            os << value;
            return os.str();
        } else {
            return "<unprintable " + std::string(compiler_type_name<T>()) + ">";
        }
    }

    // Renders `value` as a tree. The parts come from the value itself and
    // each part is rendered the same way, all the way down, so a difference
    // of two values can descend to the part that changed. A value with no
    // parts to descend into (a number, a string, an enum) is a leaf.
    template <typename T> ReprNode repr_node(const T& value) {
        ReprNode node;
        node.text = repr(value);
        if constexpr (repr_detail::is_vector<T>::value ||
                      repr_detail::is_set<T>::value ||
                      repr_detail::is_std_array<T>::value) {
            node.prefix = cpp_type_name<T>() + "{";
            for (const auto& element : value) {
                node.parts.push_back(repr_node(element));
            }
        } else if constexpr (repr_detail::is_map<T>::value) {
            node.prefix = cpp_type_name<T>() + "{";
            for (const auto& entry : value) {
                // An entry is built here rather than through repr_node,
                // which would name the pair's type. repr() renders a map
                // entry as a bare brace pair.
                ReprNode item;
                item.text =
                    "{" + repr(entry.first) + ", " + repr(entry.second) + "}";
                item.prefix = "{";
                item.parts.push_back(repr_node(entry.first));
                item.parts.push_back(repr_node(entry.second));
                node.parts.push_back(std::move(item));
            }
        } else if constexpr (repr_detail::is_pair<T>::value) {
            node.prefix = cpp_type_name<T>() + "{";
            node.parts.push_back(repr_node(value.first));
            node.parts.push_back(repr_node(value.second));
        } else if constexpr (repr_detail::is_tuple<T>::value) {
            node.prefix = cpp_type_name<T>() + "{";
            std::apply(
                [&node](const auto&... elements) {
                    (node.parts.push_back(repr_node(elements)), ...);
                },
                value);
        }
#if HEGEL_HAS_REFLECTION
        else if constexpr (repr_detail::is_reflectable<T>::value) {
            node.prefix = cpp_type_name<T>() + "{";
            auto view = rfl::to_view(const_cast<T&>(value));
            view.apply([&node](const auto& field) {
                ReprNode item = repr_node(*field.value());
                item.label = "." + std::string(field.name()) + " = ";
                node.parts.push_back(std::move(item));
            });
        }
#endif
        // A composite with no parts (an empty container) has nothing to
        // descend into, so it reads as a leaf.
        if (node.parts.empty()) {
            node.prefix.clear();
        }
        return node;
    }

} // namespace hegel::internal

/// @endcond
