#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "cereal/schema/SchemaDescription.h"
#include "models.h"

namespace proto {

struct Type;
struct Enum;
struct Field;
struct Packet;
struct TypeAlias;
using TypeRef = std::variant<std::string, Type, Enum, std::reference_wrapper<const TypeAlias>>;

using Repeat = std::variant<std::uint64_t, std::string>;  // count | type

struct ArraySpec;
struct MapSpec;
struct VariantSpec;
using TypeSpec =
    std::variant<TypeRef, std::shared_ptr<ArraySpec>, std::shared_ptr<MapSpec>, std::shared_ptr<VariantSpec>>;

struct EnumField;
struct VariantField;
struct ArrayField;
struct MapField;
using FieldType = std::variant<Field, EnumField, VariantField, ArrayField, MapField>;

template <typename Derived>
struct Model {
    std::string name;

    template <typename T>
    [[nodiscard]] static constexpr bool is()
    {
        return std::is_base_of_v<T, Derived>;
    }
};

struct Type : Model<Type> {
    std::vector<FieldType> fields;
    bool no_output = false;
};

struct Packet : Model<Packet> {
    int id = 0;
    std::optional<std::string> notes;
    Type payload;
};

struct Enum : Model<Enum> {
    std::vector<std::pair<std::string, std::int64_t>> values;
};

struct TypeAlias : Model<TypeAlias> {
    TypeRef value;
};

template <typename Derived>
struct FieldBase : Model<Derived> {
    std::optional<std::string> description;
    std::optional<cereal::internal::ConstraintDescription> constraints;
    bool optional = false;
    bool deprecated = false;
};

struct Field : FieldBase<Field> {
    TypeRef type;
};

struct EnumField : FieldBase<EnumField> {
    TypeRef type;
    TypeRef enum_type;
};

struct SwitchOn {
    std::string type;
    std::optional<std::string> name;
    std::optional<TypeRef> enum_type;
};

struct VariantField : FieldBase<VariantField> {
    SwitchOn switch_on;
    std::vector<TypeRef> cases;
};

struct ArraySpec {
    Repeat repeat;
    TypeSpec element_type;
};

struct MapSpec {
    TypeSpec key_type;
    TypeSpec value_type;
};

struct VariantSpec {
    SwitchOn switch_on;
    std::vector<TypeRef> cases;
};

struct ArrayField : FieldBase<ArrayField> {
    Repeat repeat;
    TypeSpec element_type;
};

struct MapField : FieldBase<MapField> {
    TypeSpec key_type;
    TypeSpec value_type;
};

}  // namespace proto

template <>
struct nlohmann::adl_serializer<cereal::internal::ConstraintDescription> {
    static ordered_json as_number(double v)
    {
        auto s = std::to_string(v);
        if (s.find_last_not_of('0') == s.find('.')) {
            return static_cast<std::int64_t>(v);
        }
        return v;
    }

    static void to_json(ordered_json &j, const cereal::internal::ConstraintDescription &c)
    {
        j = ordered_json::object();
        if (c.mMultipleOf) {
            j["multiple_of"] = as_number(*c.mMultipleOf);
        }
        if (c.mMinimum) {
            j["minimum"] = as_number(*c.mMinimum);
        }
        if (c.mExclusiveMinimum) {
            j["exclusive_minimum"] = as_number(*c.mExclusiveMinimum);
        }
        if (c.mMaximum) {
            j["maximum"] = as_number(*c.mMaximum);
        }
        if (c.mExclusiveMaximum) {
            j["exclusive_maximum"] = as_number(*c.mExclusiveMaximum);
        }
        if (c.mMinLength) {
            j["min_length"] = *c.mMinLength;
        }
        if (c.mMaxLength) {
            j["max_length"] = *c.mMaxLength;
        }
        if (c.mPattern) {
            j["pattern"] = *c.mPattern;
        }
        if (c.mRegexFlags) {
            j["regex_flags"] = *c.mRegexFlags;
        }
        if (c.mMinItems) {
            j["min_items"] = *c.mMinItems;
        }
        if (c.mMaxItems) {
            j["max_items"] = *c.mMaxItems;
        }
        if (c.mNoDuplicates) {
            j["no_duplicates"] = *c.mNoDuplicates;
        }
        if (c.mItems) {
            j["items"] = *c.mItems;
        }
        if (c.mMinProperties) {
            j["min_properties"] = *c.mMinProperties;
        }
        if (c.mMaxProperties) {
            j["max_properties"] = *c.mMaxProperties;
        }
        if (c.mPropertyNames) {
            j["property_names"] = *c.mPropertyNames;
        }
        if (c.mAdditionalProperties) {
            j["additional_properties"] = *c.mAdditionalProperties;
        }
        if (c.mEnumValues) {
            j["enum_values"] = *c.mEnumValues;
        }
        if (!c.mVariantTypes.empty()) {
            auto arr = ordered_json::array();
            for (const auto &v : c.mVariantTypes) {
                arr.push_back(v ? ordered_json(*v) : ordered_json());
            }
            j["variant_types"] = std::move(arr);
        }
        if (c.mCustomDescription) {
            j["description"] = *c.mCustomDescription;
        }
    }
};

template <>
struct nlohmann::adl_serializer<proto::FieldType> {
    static void to_json(ordered_json &j, const proto::FieldType &ft);
};

template <>
struct nlohmann::adl_serializer<proto::Type> {
    static void to_json(ordered_json &j, const proto::Type &t)
    {
        j["name"] = t.name;
        j["fields"] = t.fields;
    }
};

template <>
struct nlohmann::adl_serializer<proto::Enum> {
    static void to_json(ordered_json &j, const proto::Enum &e)
    {
        j["name"] = e.name;
        j["values"] = ordered_json::array();
        for (const auto &[n, v] : e.values) {
            j["values"].push_back({
                {"name", n},
                {"value", v},
            });
        }
    }
};

template <>
struct nlohmann::adl_serializer<proto::Packet> {
    static void to_json(ordered_json &j, const proto::Packet &p)
    {
        j["id"] = p.id;
        j["name"] = p.name;
        if (p.notes.has_value()) {
            j["notes"] = p.notes.value();
        }
        j["fields"] = p.payload.fields;
    }
};

template <>
struct nlohmann::adl_serializer<proto::TypeAlias> {
    static void to_json(ordered_json &j, const proto::TypeAlias &a)
    {
        std::visit(
            [&](auto &&v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    j = v;
                }
                else if constexpr (std::is_same_v<T, std::reference_wrapper<const proto::TypeAlias>>) {
                    j = v.get();
                }
                else {
                    j = v.name;
                }
            },
            a.value);
    }
};

template <>
struct nlohmann::adl_serializer<proto::TypeRef> {
    static void to_json(ordered_json &j, const proto::TypeRef &ref)
    {
        std::visit(
            [&](auto &&v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    if (v == "cereal::NullType") {
                        j = ordered_json();
                    }
                    else {
                        j = v;
                    }
                }
                else if constexpr (std::is_same_v<T, std::reference_wrapper<const proto::TypeAlias>>) {
                    j = v.get();
                }
                else {
                    j = v.name;
                }
            },
            ref);
    }
};

template <>
struct nlohmann::adl_serializer<proto::SwitchOn> {
    static void to_json(ordered_json &j, const proto::SwitchOn &s)
    {
        j = ordered_json::object();
        if (s.name) {
            j["name"] = *s.name;
        }
        j["type"] = s.type;
        if (s.enum_type) {
            j["enum"] = *s.enum_type;
        }
    }
};

template <>
struct nlohmann::adl_serializer<proto::Repeat> {
    static void to_json(ordered_json &j, const proto::Repeat &r)
    {
        j = ordered_json::object();
        std::visit(
            [&](auto &&v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::uint64_t>) {
                    j["count"] = v;
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    j["prefix"] = v;
                }
            },
            r);
    }
};

template <>
struct nlohmann::adl_serializer<proto::TypeSpec> {
    static void to_json(ordered_json &j, const proto::TypeSpec &s)
    {
        std::visit(
            [&](auto &&v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, proto::TypeRef>) {
                    j = v;
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<proto::ArraySpec>>) {
                    j = ordered_json::object();
                    j["type"] = v->element_type;
                    j["repeat"] = v->repeat;
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<proto::MapSpec>>) {
                    j = ordered_json::object();
                    j["key"] = v->key_type;
                    j["value"] = v->value_type;
                }
                else if constexpr (std::is_same_v<T, std::shared_ptr<proto::VariantSpec>>) {
                    j = ordered_json::object();
                    j["switch"] = v->switch_on;
                    j["cases"] = v->cases;
                }
            },
            s);
    }
};

inline void nlohmann::adl_serializer<
    std::variant<proto::Field, proto::EnumField, proto::VariantField, proto::ArrayField, proto::MapField>,
    void>::to_json(ordered_json &j, const proto::FieldType &ft)
{
    std::visit(
        [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;

            j["name"] = arg.name;

            if constexpr (std::is_same_v<T, proto::Field>) {
                j["type"] = arg.type;
            }
            else if constexpr (std::is_same_v<T, proto::EnumField>) {
                j["type"] = arg.type;
                j["enum"] = arg.enum_type;
            }
            else if constexpr (std::is_same_v<T, proto::VariantField>) {
                j["type"] = {
                    {"switch", arg.switch_on},
                    {"cases", arg.cases},
                };
            }
            else if constexpr (std::is_same_v<T, proto::ArrayField>) {
                j["type"] = arg.element_type;
                j["repeat"] = arg.repeat;
            }
            else if constexpr (std::is_same_v<T, proto::MapField>) {
                j["type"] = ordered_json::object();
                j["type"]["key"] = arg.key_type;
                j["type"]["value"] = arg.value_type;
            }

            if (arg.description) {
                j["description"] = *arg.description;
            }
            if (arg.constraints) {
                auto cs = ordered_json(*arg.constraints);
                if (!cs.empty()) {
                    j["constraints"] = std::move(cs);
                }
            }
            if (arg.optional) {
                j["optional"] = true;
            }
            if (arg.deprecated) {
                j["deprecated"] = true;
            }
        },
        ft);
}
