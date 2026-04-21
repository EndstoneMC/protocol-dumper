#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "models.h"

namespace proto {

struct Type;
struct Enum;
struct Field;
struct Packet;
struct TypeAlias;

struct EnumField;
struct VariantField;
struct ArrayField;
struct MapField;
using FieldType = std::variant<Field, EnumField, VariantField, ArrayField, MapField>;

struct Constraints {
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<std::uint64_t> min_length;
    std::optional<std::uint64_t> max_length;
    std::optional<std::uint64_t> min_items;
    std::optional<std::uint64_t> max_items;
    std::optional<std::string> pattern;
    [[nodiscard]] bool empty() const
    {
        return !minimum && !maximum && !min_length && !max_length && !min_items && !max_items && !pattern;
    }
};

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
};

struct Packet : Model<Packet> {
    int id = 0;
    std::optional<std::string> description;
    Type payload;
};

struct TypeAlias : Model<TypeAlias> {
    using ValueType = std::variant<std::string, Type>;
    ValueType value;
};

struct Enum : Model<Enum> {
    std::vector<std::pair<std::string, std::int64_t>> values;
};

using TypeRef = std::variant<Type, Enum, TypeAlias>;

template <typename Derived>
struct FieldBase : Model<Derived> {
    std::optional<std::string> description;
    std::optional<Constraints> constraints;
    bool optional = false;
    bool deprecated = false;
};

struct Field : FieldBase<Field> {
    TypeRef type;
};

struct EnumField : FieldBase<EnumField> {
    TypeRef enum_type;
};

struct VariantField : FieldBase<VariantField> {
    TypeRef switch_on;
    std::vector<TypeRef> cases;
};

struct ArrayField : FieldBase<ArrayField> {
    TypeRef repeat;
    TypeRef element_type;
};

struct MapField : FieldBase<MapField> {
    TypeRef key_type;
    TypeRef value_type;
};

}  // namespace proto

template <>
struct nlohmann::adl_serializer<proto::Constraints> {
    static void to_json(ordered_json &j, const proto::Constraints &c)
    {
        j = ordered_json::object();
        if (c.minimum) {
            j["minimum"] = *c.minimum;
        }
        if (c.maximum) {
            j["maximum"] = *c.maximum;
        }
        if (c.min_length) {
            j["min_length"] = *c.min_length;
        }
        if (c.max_length) {
            j["max_length"] = *c.max_length;
        }
        if (c.min_items) {
            j["min_items"] = *c.min_items;
        }
        if (c.max_items) {
            j["max_items"] = *c.max_items;
        }
        if (c.pattern) {
            j["pattern"] = *c.pattern;
        }
    }
};

template <>
struct nlohmann::adl_serializer<proto::Field> {
    static void to_json(ordered_json &j, const proto::Field &f);
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
        j["kind"] = "enum";
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
        if (p.description.has_value()) {
            j["description"] = p.description.value();
        }
        j["fields"] = p.payload.fields;
    }
};

template <>
struct nlohmann::adl_serializer<proto::TypeAlias> {
    static void to_json(ordered_json &j, const proto::TypeAlias &a)
    {
        std::visit([&](auto &&v) { j = v; }, a.value);
    }
};

template <>
struct nlohmann::adl_serializer<proto::TypeRef> {
    static void to_json(ordered_json &j, const proto::TypeRef &ref)
    {
        std::visit([&](auto &&v) { j = v; }, ref);
    }
};
