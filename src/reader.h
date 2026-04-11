#pragma once

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "cereal/schema/SchemaDescription.h"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

// Reads cereal SchemaDescription trees from BDS's live ReflectionCtx.
//
// Approach: BDS populates a global ReflectionCtx at startup via
// cerealizer<T>::bind() calls.  The ctx wraps an entt::meta_ctx whose
// registered types include every packet payload.  We iterate those types,
// filter for *PacketPayload names, and call BasicSchema::lookup() +
// description() to extract the schema tree for each.
//
// All three BDS functions are resolved by RVA at init() time.

class CerealSchemaReader {
public:
    using LogFn = std::function<void(const std::string &)>;

    // Resolves RVAs and obtains the global ReflectionCtx.
    // Returns false if any resolution step fails.
    // log_fn receives diagnostic messages.
    bool init(uintptr_t base_addr, LogFn log_fn = {});

    // Extract the cereal SchemaDescription for a single type by name.
    // The name should be the entt-registered type name as it appears in
    // the meta_ctx (e.g. "GameRulesChangedPacketPayload").
    std::optional<cereal::SchemaDescription> getSchema(const std::string &type_name);

    // Extract schemas for all registered types whose name contains
    // the given filter substring (e.g. "PacketPayload").
    // Returns map<type_name, SchemaDescription>.
    std::map<std::string, cereal::SchemaDescription> getAllSchemas(
        const std::string &name_filter = "PacketPayload");

    // Dump all registered type names to a file (diagnostic).
    void dumpRegisteredTypes(const std::filesystem::path &output_dir);

private:
    // Function pointer types for BDS functions resolved via RVA
    using ReflectionCtxGlobalFn = cereal::ReflectionCtx &(*)();
    using BasicSchemaLookupFn = const cereal::internal::BasicSchema &(*)(
        const entt::meta_ctx &, entt::type_info);
    // description() is a non-virtual member — called via thiscall convention.
    // On MSVC x64, this is just a regular call with `this` as first arg.
    using BasicSchemaDescFn = cereal::SchemaDescription (*)(
        const cereal::internal::BasicSchema *self,
        const cereal::internal::ReflectionContext &ctx,
        cereal::DescriptionConfig config);

    ReflectionCtxGlobalFn reflectionCtxGlobal_ = nullptr;
    BasicSchemaLookupFn basicSchemaLookup_ = nullptr;
    BasicSchemaDescFn basicSchemaDesc_ = nullptr;

    cereal::ReflectionCtx *ctx_ = nullptr;
    LogFn log_;

    void log(const std::string &msg);
};
