#include "BasicSchema.h"

#include <spdlog/spdlog.h>

const cereal::internal::BasicSchema &cereal::internal::BasicSchema::lookup(const entt::meta_ctx &ctx,
                                                                           entt::type_info info)
{
    if (auto type = entt::resolve(ctx, info)) {
        if (auto *schema = static_cast<const BasicSchema *const *>(type.custom())) {
            if (*schema) {
                return **schema;
            }
        }
    }

    spdlog::error("BasicSchema::lookup failed for type hash={}", info.hash());
    std::terminate();
}

cereal::SchemaDescription cereal::internal::BasicSchema::description(const cereal::internal::ReflectionContext &ctx,
                                                                     cereal::DescriptionConfig config) const
{
    return makeDescription(ctx, config);
}
