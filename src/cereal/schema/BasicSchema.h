#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/BasicSchema.h

#include <entt/entt.hpp>

#include "SchemaDescription.h"

namespace cereal {
namespace internal {

struct ReflectionContext;

class BasicSchema {
public:
    virtual ~BasicSchema() = default;

    static const BasicSchema &lookup(const entt::meta_ctx &ctx, entt::type_info info);
    SchemaDescription description(const ReflectionContext &ctx, DescriptionConfig config) const;

    struct TypeDescriptor {};
    struct MemberDescriptor {};
};

}  // namespace internal
}  // namespace cereal
