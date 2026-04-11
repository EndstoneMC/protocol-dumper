#include "reader.h"

#include <fstream>

// ============================================================================
// RVAs for cereal functions in BDS
// These are version-specific.  Update when BDS updates.
// Find via Endstone bedrock_symbols.generated.h or IDA/Ghidra.
//
// Symbols (MSVC mangled):
//   ReflectionCtx::global   ?global@ReflectionCtx@cereal@@SAAEAV12@XZ
//   BasicSchema::lookup     ?lookup@BasicSchema@internal@cereal@@...
//   BasicSchema::description ?description@BasicSchema@internal@cereal@@...
// ============================================================================

// TODO: Fill in actual RVAs from your BDS version's symbol table.
// Set to 0 to disable cereal extraction (graceful fallback).
constexpr uintptr_t REFLECTION_CTX_GLOBAL_RVA = 0;
constexpr uintptr_t BASIC_SCHEMA_LOOKUP_RVA = 0;
constexpr uintptr_t BASIC_SCHEMA_DESC_RVA = 0;

// ============================================================================
// init
// ============================================================================

void CerealSchemaReader::log(const std::string &msg)
{
    if (log_) log_(msg);
}

bool CerealSchemaReader::init(uintptr_t base_addr, LogFn log_fn)
{
    log_ = std::move(log_fn);

    if (REFLECTION_CTX_GLOBAL_RVA == 0 ||
        BASIC_SCHEMA_LOOKUP_RVA == 0 ||
        BASIC_SCHEMA_DESC_RVA == 0) {
        log("[cereal] RVAs not configured — cereal schema extraction disabled");
        return false;
    }

    reflectionCtxGlobal_ = reinterpret_cast<ReflectionCtxGlobalFn>(
        base_addr + REFLECTION_CTX_GLOBAL_RVA);
    basicSchemaLookup_ = reinterpret_cast<BasicSchemaLookupFn>(
        base_addr + BASIC_SCHEMA_LOOKUP_RVA);
    basicSchemaDesc_ = reinterpret_cast<BasicSchemaDescFn>(
        base_addr + BASIC_SCHEMA_DESC_RVA);

    log("[cereal] Resolving ReflectionCtx::global()...");

    try {
        ctx_ = &reflectionCtxGlobal_();
    }
    catch (...) {
        log("[cereal] ERR: ReflectionCtx::global() threw");
        return false;
    }

    if (!ctx_) {
        log("[cereal] ERR: ReflectionCtx::global() returned null");
        return false;
    }

    // Quick sanity check: count registered types
    auto &meta_ctx = ctx_->internal().mMetaCtx;
    int type_count = 0;
    for ([[maybe_unused]] auto &&[id, type] : entt::resolve(meta_ctx)) {
        type_count++;
    }
    log("[cereal] ReflectionCtx has " + std::to_string(type_count) + " registered types");

    return true;
}

// ============================================================================
// getSchema
// ============================================================================

std::optional<cereal::SchemaDescription> CerealSchemaReader::getSchema(
    const std::string &type_name)
{
    if (!ctx_ || !basicSchemaLookup_ || !basicSchemaDesc_) return std::nullopt;

    auto &meta_ctx = ctx_->internal().mMetaCtx;

    // Search registered types for the one matching type_name
    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto info = meta_type.info();
        std::string_view name = info.name();

        // entt type names may include namespace qualifiers or MSVC decoration.
        // Check for exact match or suffix match.
        if (name == type_name ||
            (name.size() > type_name.size() &&
             name.substr(name.size() - type_name.size()) == type_name)) {

            try {
                const auto &schema = basicSchemaLookup_(meta_ctx, info);

                cereal::DescriptionConfig config{};
                config.mContextArea = cereal::ContextArea::ALL;
                config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
                config.mIsTopLevel = true;

                return basicSchemaDesc_(&schema, ctx_->internal(), config);
            }
            catch (...) {
                log("[cereal] ERR: exception extracting schema for " +
                    std::string(name));
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

// ============================================================================
// getAllSchemas
// ============================================================================

std::map<std::string, cereal::SchemaDescription> CerealSchemaReader::getAllSchemas(
    const std::string &name_filter)
{
    std::map<std::string, cereal::SchemaDescription> results;
    if (!ctx_ || !basicSchemaLookup_ || !basicSchemaDesc_) return results;

    auto &meta_ctx = ctx_->internal().mMetaCtx;

    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto info = meta_type.info();
        std::string name(info.name());

        if (name.find(name_filter) == std::string::npos) continue;

        try {
            const auto &schema = basicSchemaLookup_(meta_ctx, info);

            cereal::DescriptionConfig config{};
            config.mContextArea = cereal::ContextArea::ALL;
            config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
            config.mIsTopLevel = true;

            results.emplace(name, basicSchemaDesc_(&schema, ctx_->internal(), config));
        }
        catch (...) {
            log("[cereal] ERR: exception extracting schema for " + name);
        }
    }

    return results;
}

// ============================================================================
// dumpRegisteredTypes — diagnostic: list all types in the meta_ctx
// ============================================================================

void CerealSchemaReader::dumpRegisteredTypes(const std::filesystem::path &output_dir)
{
    if (!ctx_) return;

    std::ofstream out(output_dir / "cereal_types.txt");
    out << "=== Registered entt meta types ===\n\n";

    auto &meta_ctx = ctx_->internal().mMetaCtx;
    int count = 0;

    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto info = meta_type.info();
        out << "[" << count++ << "] id=" << id
            << "  hash=" << info.hash()
            << "  name=" << info.name() << "\n";

        // List data members
        for (auto &&[data_id, data] : meta_type.data()) {
            out << "    data: id=" << data_id;
            if (data.type()) {
                out << "  type_hash=" << data.type().info().hash()
                    << "  type_name=" << data.type().info().name();
            }
            out << "\n";
        }
    }

    out << "\nTotal: " << count << " types\n";
}
