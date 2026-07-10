#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/Constraint.h

#include <cstddef>

#include <entt/entt.hpp>

#include "schema/SchemaDescription.h"
#include "schema/SerializationTraits.h"
#include "version.h"

namespace cereal {

class SerializerContext;

namespace internal {

struct InputConstraint {
    std::size_t mMaxLength;
#if BEDROCK_SERVER_VERSION_HEX >= 0x011A1E00  // 1.26.30
    std::size_t mMinLength;
#endif
};

}  // namespace internal

class Constraint {
public:
    using Description = internal::ConstraintDescription;
    using TypeInfo = entt::type_info;

    virtual void doValidate(const entt::meta_any &, SerializerContext &) const = 0;
    virtual Description doDescription(ContextArea) const = 0;
#if BEDROCK_SERVER_VERSION_HEX >= 0x011A1E00  // 1.26.30
    virtual std::size_t doMaxInputLength() const;
    virtual std::size_t doMinInputLength() const;
#endif
    virtual ~Constraint();
    virtual const Constraint *subConstraint(std::size_t) const;
    virtual const TypeInfo &info() const = 0;

private:
    internal::InputConstraint mInputConstraint;
    ContextArea mContextArea;
};

}  // namespace cereal
