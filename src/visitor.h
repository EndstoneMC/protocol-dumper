#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cereal/Context.h"
#include "cereal/schema/SchemaDescription.h"
#include "models.h"

namespace proto {

class Visitor {
public:
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };
    using AliasMap = std::unordered_map<std::string, const cereal::SchemaDescription *, StringHash, std::equal_to<>>;

    Visitor(const cereal::ReflectionCtx &ctx, const cereal::DescriptionConfig &config, AliasMap aliases);

    model::TypeDef visitType(const std::string &name, const cereal::SchemaDescription &desc);
    model::Packet visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc);

private:
    struct Resolved {
        model::TypeSpec spec;
        std::string enum_name;
        std::string repeat;
    };

    Resolved visit(const cereal::SchemaDescription &desc);
    Resolved visitScalar(const cereal::SchemaDescription &desc);
    Resolved visitEnum(const cereal::SchemaDescription &desc);
    Resolved visitObject(const cereal::SchemaDescription &desc);
    Resolved visitArray(const cereal::SchemaDescription &desc);
    Resolved visitMap(const cereal::SchemaDescription &desc);
    Resolved buildVariant(const entt::meta_type &variantType);

    model::Field visitField(const std::string &name, const cereal::internal::Member &member,
                            const entt::meta_type &parent_meta);
    std::vector<model::Field> visitStructFields(const cereal::SchemaDescription &desc);

    AliasMap mAliases;
    const cereal::ReflectionCtx &mCtx;
    cereal::DescriptionConfig mConfig;
    const entt::meta_ctx &mMetaCtx;
};

struct DumpResult {
    std::vector<model::TypeDef> types;
    std::vector<model::Packet> packets;
};

DumpResult dumpProtocol(const cereal::ReflectionCtx &ctx, const cereal::DescriptionConfig &config);

}  // namespace proto
