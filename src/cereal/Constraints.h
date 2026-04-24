#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/Constraint.h

#include <cstddef>

#include <entt/entt.hpp>

#include "schema/SchemaDescription.h"

namespace cereal {

class SerializerContext;  // forward decl; only appears in a signature we never call

namespace internal {

struct InputConstraint {
    std::size_t mMaxLength;
};

}  // namespace internal

class Constraint {
public:
    using Description = internal::ConstraintDescription;
    using TypeInfo = entt::type_info;

    // Virtual declaration order MUST match BDS so vtable slots line up.
    virtual void doValidate(const entt::meta_any &, SerializerContext &) const = 0;
    virtual Description doDescription(ContextArea) const = 0;
    virtual ~Constraint();
    virtual const Constraint *subConstraint(std::size_t) const;
    virtual const TypeInfo &info() const = 0;

private:
    internal::InputConstraint mInputConstraint;
    ContextArea mContextArea;
};

}  // namespace cereal
