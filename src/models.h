#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace proto {

struct Type;
struct Enum;
struct Field;
struct Map;
struct Packet;
struct TypeAlias;
struct Variant;

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
    std::vector<Field> fields;
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

struct Variant : Model<Variant> {
    TypeRef switch_on;
    std::vector<TypeRef> cases;
};

struct Map : Model<Map> {
    TypeRef key_type;
    TypeRef value_type;
};

struct Field : Model<Field> {
    std::variant<Type, Variant, Map> type;
    std::optional<std::string> enum_name;
    std::optional<std::string> repeat;
    std::optional<Constraints> constraints;
    bool optional = false;
    bool deprecated = false;
    std::string description;
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

template <>
struct nlohmann::adl_serializer<proto::Variant> {
    static void to_json(ordered_json &j, const proto::Variant &v)
    {
        j["switch"] = v.switch_on;
        j["cases"] = v.cases;
    }
};

template <>
struct nlohmann::adl_serializer<proto::Map> {
    static void to_json(ordered_json &j, const proto::Map &m)
    {
        j["key"] = m.key_type;
        j["value"] = m.value_type;
    }
};

inline void nlohmann::adl_serializer<proto::Field>::to_json(ordered_json &j, const proto::Field &f)
{
    j["name"] = f.name;
    std::visit(
        [&](auto &&v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, proto::Type>) {
                j["type"] = v.name;
            }
            else {
                j["type"] = v;
            }
        },
        f.type);
    if (f.enum_name) {
        j["enum"] = *f.enum_name;
    }
    if (f.repeat) {
        j["repeat"] = *f.repeat;
    }
    if (f.optional) {
        j["optional"] = true;
    }
    if (f.deprecated) {
        j["deprecated"] = true;
    }
    if (!f.description.empty()) {
        j["description"] = f.description;
    }
    if (f.constraints && !f.constraints->empty()) {
        j["constraints"] = *f.constraints;
    }
}
