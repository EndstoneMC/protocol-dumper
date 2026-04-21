#pragma once

#include <map>

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "cereal/schema/SchemaDescription.h"
#include "models.h"

namespace proto {

class Visitor {
    using TypeMap = std::map<entt::id_type, std::variant<Type, Enum, Packet, TypeAlias>>;

public:
    explicit Visitor(const cereal::ReflectionCtx &ctx);

    [[nodiscard]] const TypeMap &getTypes() const;

private:
    void visit(const entt::meta_type &type);
    void visitPacket(const entt::meta_type &type);
    void visitTypeAlias(const entt::meta_type &type, const entt::meta_type &value);
    void visitEnum(const entt::meta_type &type, const cereal::SchemaDescription &desc);
    void visitType(const entt::meta_type &type, const cereal::SchemaDescription &desc);

    [[nodiscard]] bool isVisited(const entt::meta_type &type) const;

    const cereal::ReflectionCtx &reflection_ctx_;
    const entt::meta_ctx &meta_ctx_;
    cereal::DescriptionConfig config_;
    TypeMap types_;
};

}  // namespace proto
