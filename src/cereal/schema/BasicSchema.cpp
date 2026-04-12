#include "BasicSchema.h"

const cereal::internal::BasicSchema &cereal::internal::BasicSchema::lookup(const entt::meta_ctx &ctx,
                                                                           entt::type_info info)
{
    auto type = entt::resolve(ctx, info);
    if (!type) {
        throw std::runtime_error(std::format("Failed to resolve type {}", info.name()));
    }
    TypeDescriptor &descriptor = type.custom();
    return *descriptor.mPtr;
}

cereal::SchemaDescription cereal::internal::BasicSchema::description(const ReflectionContext &ctx,
                                                                     const DescriptionConfig config) const
{
    return makeDescription(ctx, config);
}
