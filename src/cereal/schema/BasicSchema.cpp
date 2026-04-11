#include "BasicSchema.h"

#include "../../symbol.h"

const cereal::internal::BasicSchema &cereal::internal::BasicSchema::lookup(
    const entt::meta_ctx &ctx, entt::type_info info)
{
    return BEDROCK_CALL(&BasicSchema::lookup, ctx, info);
}

cereal::SchemaDescription cereal::internal::BasicSchema::description(
    const cereal::internal::ReflectionContext &ctx, cereal::DescriptionConfig config) const
{
    return BEDROCK_CALL(&BasicSchema::description, this, ctx, config);
}
