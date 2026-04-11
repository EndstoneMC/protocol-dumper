#include "reader.h"

#include <fstream>

// RVAs for cereal functions in BDS — version-specific.
// Find via Endstone bedrock_symbols.generated.h or IDA/Ghidra.
// Set to 0 to disable cereal extraction (graceful fallback).
constexpr uintptr_t REFLECTION_CTX_GLOBAL_RVA = 0;
constexpr uintptr_t BASIC_SCHEMA_LOOKUP_RVA = 0;
constexpr uintptr_t BASIC_SCHEMA_DESC_RVA = 0;

void CerealSchemaReader::log(const std::string &msg)
{
    if (log_) log_(msg);
}

std::optional<cereal::SchemaDescription> CerealSchemaReader::extractSchema(
    entt::type_info info)
{
    auto &meta_ctx = ctx_->internal().mMetaCtx;
    try {
        const auto &schema = basicSchemaLookup_(meta_ctx, info);
        cereal::DescriptionConfig config{};
        config.mContextArea = cereal::ContextArea::ALL;
        config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
        config.mIsTopLevel = true;
        return basicSchemaDesc_(&schema, ctx_->internal(), config);
    }
    catch (...) {
        log("[cereal] ERR: exception extracting schema for " + std::string(info.name()));
        return std::nullopt;
    }
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

    // Build name → type_info index for O(1) lookups
    auto &meta_ctx = ctx_->internal().mMetaCtx;
    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto info = meta_type.info();
        std::string name(info.name());
        type_index_.emplace(std::move(name), info);
    }

    log("[cereal] ReflectionCtx has " + std::to_string(type_index_.size()) + " registered types");
    return true;
}

std::optional<cereal::SchemaDescription> CerealSchemaReader::getSchema(
    const std::string &type_name)
{
    if (!ctx_) return std::nullopt;

    // Exact match
    auto it = type_index_.find(type_name);
    if (it != type_index_.end()) return extractSchema(it->second);

    // Suffix match (handles namespace-qualified names)
    for (const auto &[name, info] : type_index_) {
        if (name.size() > type_name.size() &&
            name.compare(name.size() - type_name.size(), type_name.size(), type_name) == 0) {
            return extractSchema(info);
        }
    }

    return std::nullopt;
}

std::map<std::string, cereal::SchemaDescription> CerealSchemaReader::getAllSchemas(
    const std::string &name_filter)
{
    std::map<std::string, cereal::SchemaDescription> results;
    if (!ctx_) return results;

    for (const auto &[name, info] : type_index_) {
        if (name.find(name_filter) == std::string::npos) continue;
        if (auto desc = extractSchema(info)) {
            results.emplace(name, std::move(*desc));
        }
    }

    return results;
}

void CerealSchemaReader::dumpRegisteredTypes(const std::filesystem::path &output_dir)
{
    if (!ctx_) return;

    std::ofstream out(output_dir / "cereal_types.txt");
    int count = 0;

    auto &meta_ctx = ctx_->internal().mMetaCtx;
    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto info = meta_type.info();
        out << "[" << count++ << "] id=" << id
            << "  hash=" << info.hash()
            << "  name=" << info.name() << "\n";

        for (auto &&[data_id, data] : meta_type.data()) {
            out << "    data: id=" << data_id;
            if (data.type()) {
                out << "  type=" << data.type().info().name();
            }
            out << "\n";
        }
    }

    out << "\nTotal: " << count << " types\n";
}
