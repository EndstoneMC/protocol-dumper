#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/BasicSchema.h
//
// lookup() and description() are implemented by us directly — no sigscan needed.
// lookup() reads the schema from the entt meta_type_node's custom data.
// description() delegates to virtual makeDescription() on the BDS-owned object.

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

private:
    virtual bool isGreedy(const entt::meta_ctx &) const { return false; }
    virtual void doLoad() const {}
    virtual void doSave() const {}
    virtual SchemaDescription makeDescription(const ReflectionContext &ctx, DescriptionConfig config) const = 0;
};

}  // namespace internal
}  // namespace cereal
