#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

    Visitor(const entt::meta_ctx &ctx, AliasMap aliases);
    virtual ~Visitor() = default;

    [[nodiscard]] const std::vector<model::Packet> &packets() const { return mPackets; }
    [[nodiscard]] const std::vector<std::unique_ptr<model::Class>> &classes() const { return mClasses; }

    void visitClass(const std::string &name, const cereal::SchemaDescription &desc);
    void visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc);

protected:
    virtual void visit(const cereal::SchemaDescription &desc);
    virtual void visitScalar(const cereal::SchemaDescription &desc);
    virtual void visitEnum(const cereal::SchemaDescription &desc);
    virtual void visitObject(const cereal::SchemaDescription &desc);
    virtual void visitArray(const cereal::SchemaDescription &desc);
    virtual void visitMap(const cereal::SchemaDescription &desc);

    virtual void visitField(const std::string &name, const cereal::internal::Member &member);

private:
    [[nodiscard]] bool isAlias(std::string_view name) const;
    void visitStructFields(const cereal::SchemaDescription &desc);

    std::vector<model::Packet> mPackets;
    std::vector<std::unique_ptr<model::Class>> mClasses;
    AliasMap mAliases;

    const entt::meta_ctx &mMetaCtx;
    model::Class *mCurrentClass = nullptr;
    model::Packet *mCurrentPacket = nullptr;
    entt::meta_type mCurrentMetaType{};
    std::unique_ptr<model::FieldType> *mTypeSlot = nullptr;
};

}  // namespace proto
