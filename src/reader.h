#pragma once

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "cereal/schema/SchemaDescription.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

class CerealSchemaReader {
public:
    bool init(uintptr_t base_addr);

    std::optional<cereal::SchemaDescription> getSchema(const std::string &type_name);

    std::map<std::string, cereal::SchemaDescription> getAllSchemas(
        const std::string &name_filter = "PacketPayload");

    void dumpRegisteredTypes(const std::filesystem::path &output_dir);

private:
    using ReflectionCtxGlobalFn = cereal::ReflectionCtx &(*)();
    using BasicSchemaLookupFn = const cereal::internal::BasicSchema &(*)(
        const entt::meta_ctx &, entt::type_info);
    using BasicSchemaDescFn = cereal::SchemaDescription (*)(
        const cereal::internal::BasicSchema *self,
        const cereal::internal::ReflectionContext &ctx,
        cereal::DescriptionConfig config);

    ReflectionCtxGlobalFn reflectionCtxGlobal_ = nullptr;
    BasicSchemaLookupFn basicSchemaLookup_ = nullptr;
    BasicSchemaDescFn basicSchemaDesc_ = nullptr;

    cereal::ReflectionCtx *ctx_ = nullptr;
    std::unordered_map<std::string, entt::type_info> type_index_;

    std::optional<cereal::SchemaDescription> extractSchema(entt::type_info info);
};
