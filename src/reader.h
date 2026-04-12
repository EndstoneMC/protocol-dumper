#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "cereal/schema/SchemaDescription.h"

class CerealSchemaReader {
public:
    explicit CerealSchemaReader(const cereal::ReflectionCtx &ctx);
    std::optional<cereal::SchemaDescription> getSchema(const std::string &type_name);

    std::map<std::string, cereal::SchemaDescription> getAllSchemas(const std::string &name_filter = "PacketPayload");

    void dumpRegisteredTypes(const std::filesystem::path &output_dir);

private:
    const cereal::ReflectionCtx &ctx_;
    std::unordered_map<std::string, entt::type_info> type_index_;

    std::optional<cereal::SchemaDescription> extractSchema(entt::type_info info);
};
