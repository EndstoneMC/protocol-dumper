#include "reader.h"

#include <fstream>

#include <spdlog/spdlog.h>

using cereal::internal::BasicSchema;

std::optional<cereal::SchemaDescription> CerealSchemaReader::extractSchema(
    entt::type_info info)
{
    auto &meta_ctx = ctx_->internal().mMetaCtx;
    try {
        const auto &schema = BasicSchema::lookup(meta_ctx, info);
        cereal::DescriptionConfig config{};
        config.mContextArea = cereal::ContextArea::ALL;
        config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
        config.mIsTopLevel = true;
        return schema.description(ctx_->internal(), config);
    }
    catch (...) {
        spdlog::error("Exception extracting schema for {}", std::string(info.name()));
        return std::nullopt;
    }
}

bool CerealSchemaReader::init(cereal::ReflectionCtx *ctx)
{
    ctx_ = ctx;
    if (!ctx_) {
        spdlog::error("ReflectionCtx is null");
        return false;
    }

    auto &meta_ctx = ctx_->internal().mMetaCtx;
    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto info = meta_type.info();
        std::string name(info.name());
        type_index_.emplace(std::move(name), info);
    }

    spdlog::info("ReflectionCtx has {} registered types", type_index_.size());
    return true;
}

std::optional<cereal::SchemaDescription> CerealSchemaReader::getSchema(
    const std::string &type_name)
{
    if (!ctx_) return std::nullopt;

    auto it = type_index_.find(type_name);
    if (it != type_index_.end()) return extractSchema(it->second);

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
