#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/SchemaDescription.h

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "DynamicValue.h"
#include "SerializationTraits.h"

namespace cereal {

namespace internal {

struct Member;  // forward declaration — defined after SchemaDescription

enum class ReflectedType : int {
    Null = 0,
    Bool = 1,
    String = 2,
    Int8 = 3,
    Uint8 = 4,
    Int16 = 5,
    Uint16 = 6,
    Int32 = 7,
    Uint32 = 8,
    Int64 = 9,
    Uint64 = 10,
    Float = 11,
    Double = 12,
    Enum = 13,
    SequenceContainer = 14,
    AssociativeContainer = 15,
    Object = 16,
};

enum class DescriptionExtra : uint8_t {
    none = 0,
    underlyingType = 1,
    controlValueType = 2,
    serializationTraits = 4,
    ordinalIndex = 8,
    numericLimits = 16,
    nonPublicFlag = 32,
    networkingExtraInfo = underlyingType | controlValueType | serializationTraits | ordinalIndex,
    _entt_enum_as_bitmask
};

struct ConstraintDescription {
    std::optional<double> mMultipleOf;
    std::optional<double> mMinimum;
    std::optional<double> mExclusiveMinimum;
    std::optional<double> mMaximum;
    std::optional<double> mExclusiveMaximum;
    std::optional<std::uint64_t> mMinLength;
    std::optional<std::uint64_t> mMaxLength;
    std::optional<std::string> mPattern;
    std::optional<std::string> mRegexFlags;
    std::optional<std::uint64_t> mMinItems;
    std::optional<std::uint64_t> mMaxItems;
    std::optional<bool> mNoDuplicates;
    std::shared_ptr<ConstraintDescription> mItems;
    std::optional<std::uint64_t> mMinProperties;
    std::optional<std::uint64_t> mMaxProperties;
    std::shared_ptr<ConstraintDescription> mPropertyNames;
    std::shared_ptr<ConstraintDescription> mAdditionalProperties;
    std::optional<std::vector<std::int64_t>> mEnumValues;
    std::vector<std::optional<ConstraintDescription>> mVariantTypes;
    std::optional<std::string> mCustomDescription;
#if BEDROCK_SERVER_VERSION_MINOR >= 26
    std::optional<std::string> mNonPublicFlag;
#endif
};

struct EnumValue {
    int64_t mValue;
    std::string mName;
    std::optional<std::string> mDescription;
    std::optional<std::map<std::string, cereal::DynamicValue>> mCerealProperties;
    std::optional<std::string> mNonPublicFlag;
};

struct SchemaInfo {
    entt::id_type mId{};
    std::optional<ReflectedType> mType;
    std::optional<std::string> mName;
    std::optional<std::string> mDescription;
};

}  // namespace internal

struct DescriptionConfig {
    using Extra = internal::DescriptionExtra;
    ContextArea mContextArea{ContextArea::ALL};
    SerializationTraits mSerializationTraits{SerializationTraits::None};
    Extra mExtraInfo;
    bool mIgnoreDeprecatedMembers;
    bool mIsTopLevel;
};

struct SchemaDescription : internal::SchemaInfo {
    using CerealProperties = std::map<std::string, DynamicValue>;
    std::optional<internal::ConstraintDescription> mConstraint;
    std::optional<std::vector<internal::EnumValue>> mEnumValues;
    std::optional<std::vector<SchemaDescription>> mParents;
    std::optional<std::vector<SchemaDescription>> mSetters;
    std::optional<std::map<std::string, internal::Member>> mMembers;
    std::optional<std::map<std::string, internal::Member>> mPatternMembers;
    std::shared_ptr<SchemaDescription> mValueType;
    std::shared_ptr<SchemaDescription> mKeyType;
    std::shared_ptr<SchemaDescription> mMappedType;
    std::optional<CerealProperties> mCerealProperties;
    std::optional<internal::ReflectedType> mUnderlyingType;
    std::optional<internal::ReflectedType> mControlValueType;
    std::optional<SerializationTraits> mSerializationTraits;
    std::optional<unsigned char> mOrdinalIndex;
    std::optional<std::string> mNonPublicFlag;
};

namespace internal {

struct Member : cereal::SchemaDescription {
    bool mRequired{false};
    bool mDeprecated{false};
    std::optional<cereal::DynamicValue> mDefaultValue;
    std::optional<cereal::DynamicValue> mConstValue;
};

}  // namespace internal

}  // namespace cereal
