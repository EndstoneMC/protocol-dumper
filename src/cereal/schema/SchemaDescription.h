#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/SchemaDescription.h
//
// Only ConstraintDescription is read back (via Constraint::doDescription). The
// dumper walks entt reflection directly and never builds a SchemaDescription,
// so the rest of that graph is omitted.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "version.h"

namespace cereal {

namespace internal {

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
#if BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 26, 0, 0)
    std::optional<std::string> mNonPublicFlag;
#endif
};

}  // namespace internal

}  // namespace cereal
