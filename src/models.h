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
    [[nodiscard]] constexpr bool is() const
    {
        return std::is_base_of_v<T, Derived>;
    }

    [[nodiscard]] constexpr std::string_view kind() const
    {
        if constexpr (is<Type>()) {
            return "class";
        }
        else if constexpr (is<Enum>()) {
            return "enum";
        }
        return "";
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

struct Variant : Model<Variant> {
    Type switch_;
    std::vector<Type> cases;
};

struct Map : Model<Map> {
    Type key_type;
    Type value_type;
};

struct Field : Model<Field> {
    Type type;
    std::optional<Constraints> constraints;
    bool optional = false;
    bool deprecated = false;
    std::string description;
};

}  // namespace proto

template <>
struct nlohmann::adl_serializer<proto::Packet> {
    static void to_json(ordered_json &j, const proto::Packet &p)
    {
        j["id"] = p.id;
        j["name"] = p.name;
        if (p.description.has_value()) {
            j["description"] = p.description.value();
        }
    }
};

template <>
struct nlohmann::adl_serializer<proto::Type> {
    static void to_json(ordered_json &j, const proto::Type &c) { j["name"] = c.name; }
};

template <>
struct nlohmann::adl_serializer<proto::TypeAlias> {
    static void to_json(ordered_json &j, const proto::TypeAlias &a)
    {
        std::visit([&](auto &&v) { j = v; }, a.value);
    }
};
