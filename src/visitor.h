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

struct Protocol {
    std::vector<TypeDef> types;
    std::vector<Packet> packets;
};

class Visitor {
public:
    explicit Visitor(const cereal::ReflectionCtx &ctx);

    Protocol dump();

private:
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };
    using AliasMap = std::unordered_map<std::string, const cereal::SchemaDescription *, StringHash, std::equal_to<>>;

    struct PacketSource {
        int id;
        std::string name;
        std::string description;
        cereal::SchemaDescription desc;
    };

    struct Resolved {
        TypeSpec spec;
        std::string enum_name;
        std::string repeat;
    };

    void collectSchemas();
    void buildAliasMap();

    TypeDef visitType(const std::string &name, const cereal::SchemaDescription &desc);
    Packet visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc);

    Resolved visit(const cereal::SchemaDescription &desc);
    Resolved visitScalar(const cereal::SchemaDescription &desc);
    Resolved visitEnum(const cereal::SchemaDescription &desc);
    Resolved visitObject(const cereal::SchemaDescription &desc);
    Resolved visitArray(const cereal::SchemaDescription &desc);
    Resolved visitMap(const cereal::SchemaDescription &desc);
    Resolved buildVariant(const entt::meta_type &variantType);

    Field visitField(const std::string &name, const cereal::internal::Member &member,
                            const entt::meta_type &parent_meta);
    std::vector<Field> visitStructFields(const cereal::SchemaDescription &desc);

    const cereal::ReflectionCtx &mCtx;
    const entt::meta_ctx &mMetaCtx;
    cereal::DescriptionConfig mConfig;
    std::vector<PacketSource> mPacketSources;
    std::unordered_map<std::string, cereal::SchemaDescription> mTypeSources;
    AliasMap mAliases;
};

}  // namespace proto
