#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace proto::model {

using Json = nlohmann::ordered_json;

struct Constraints {
    std::optional<double> mMinimum;
    std::optional<double> mMaximum;
    std::optional<std::uint64_t> mMinLength;
    std::optional<std::uint64_t> mMaxLength;
    std::optional<std::uint64_t> mMinItems;
    std::optional<std::uint64_t> mMaxItems;
    std::optional<std::string> mPattern;

    bool empty() const;
    Json toJson() const;
};

class FieldType {
public:
    virtual ~FieldType() = default;
    virtual Json toJson() const = 0;
};

class ScalarFieldType final : public FieldType {
public:
    std::string mWire;
    Json toJson() const override;
};

class EnumFieldType final : public FieldType {
public:
    std::string mTypeName;
    std::string mWire;
    Json toJson() const override;
};

class ObjectFieldType final : public FieldType {
public:
    std::string mTypeName;
    Json toJson() const override;
};

class ArrayFieldType final : public FieldType {
public:
    std::string mLengthWire;
    std::unique_ptr<FieldType> mElement;
    Json toJson() const override;
};

class MapFieldType final : public FieldType {
public:
    std::string mLengthWire;
    std::unique_ptr<FieldType> mKey;
    std::unique_ptr<FieldType> mValue;
    Json toJson() const override;
};

class VariantFieldType final : public FieldType {
public:
    struct Tag {
        std::string mName;
        std::string mTypeName;
    };
    Tag mTag;
    std::vector<std::string> mOf;
    Json toJson() const override;
};

class Member {
public:
    virtual ~Member() = default;
    virtual Json toJson() const = 0;

    std::string mName;
    std::optional<std::string> mDescription;
};

class Field final : public Member {
public:
    std::unique_ptr<FieldType> mType;
    bool mRequired{false};
    bool mDeprecated{false};
    std::optional<Constraints> mConstraints;

    Json toJson() const override;
};

class EnumValue final : public Member {
public:
    std::int64_t mValue{};

    Json toJson() const override;
};

enum class ClassKind {
    Struct,
    Enum,
};

class Class {
public:
    std::string mName;
    ClassKind mKind{ClassKind::Struct};
    std::vector<std::unique_ptr<Member>> mMembers;

    Json toJson() const;
};

class Packet {
public:
    int mId{};
    std::string mName;
    std::vector<Field> mFields;

    Json toJson() const;
};

}  // namespace proto::model
