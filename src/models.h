#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace proto {

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

struct MapType {
    std::string key;
    std::string value;
};

struct VariantType {
    std::string switch_type;
    std::string switch_name;
    std::optional<std::string> switch_enum;
    std::vector<std::string> cases;
};

using TypeSpec = std::variant<std::string, MapType, VariantType>;

struct Field {
    std::string name;
    TypeSpec type;
    std::string enum_name;
    std::string repeat;
    bool optional = false;
    bool deprecated = false;
    std::string description;
    std::optional<Constraints> constraints;
};

struct EnumEntry {
    std::string name;
    std::int64_t value{};
    std::string description;
};

struct TypeDef {
    std::string name;
    std::variant<std::vector<Field>, std::vector<EnumEntry>> body;
};

struct Packet {
    int id{};
    std::string name;
    std::string description;
    std::vector<Field> fields;
};

}  // namespace proto
