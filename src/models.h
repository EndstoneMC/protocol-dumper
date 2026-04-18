#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace proto {

struct Constraints {
    std::optional<double> mMinimum;
    std::optional<double> mMaximum;
    std::optional<std::uint64_t> mMinLength;
    std::optional<std::uint64_t> mMaxLength;
    std::optional<std::uint64_t> mMinItems;
    std::optional<std::uint64_t> mMaxItems;
    std::optional<std::string> mPattern;

    bool empty() const;
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
