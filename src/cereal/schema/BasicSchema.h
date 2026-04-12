#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/BasicSchema.h
//
// lookup() and description() are implemented by us directly — no sigscan needed.
// lookup() reads the schema from the entt meta_type_node's custom data.
// description() delegates to virtual makeDescription() on the BDS-owned object.

#include <entt/entt.hpp>

#include "SchemaDescription.h"

namespace cereal {
namespace internal {

struct ReflectionContext;

enum class VariantPriorityLevel : uint8_t {
    UNKNOWN = 0,
    OTHER = 1,
    INTEGER = 2,
    FLOAT = 3,
    DOUBLE = 4,
};

using UserPropertiesMap =
    entt::dense_map<std::string, std::pair<entt::meta_type (*)(const entt::meta_ctx &), entt::basic_any<>>>;

class BasicSchema {
public:
    struct TypeDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::string mName;
        UserPropertiesMap mUserPropertiesMap;
        std::string mErrorMessage;
    };

    virtual ~BasicSchema() = default;
    [[nodiscard]] virtual bool isGreedy(const entt::meta_ctx &) const { return false; }
    [[nodiscard]] virtual VariantPriorityLevel minVariantPriorityLevel(const entt::meta_ctx &) const
    {
        return VariantPriorityLevel::UNKNOWN;
    }
    static const BasicSchema &lookup(const entt::meta_ctx &ctx, entt::type_info info);
    [[nodiscard]] SchemaDescription description(const ReflectionContext &ctx, DescriptionConfig config) const;

private:
    virtual void doLoad() const {}
    virtual void doSave() const {}
    [[nodiscard]] virtual SchemaDescription makeDescription(const ReflectionContext &ctx, DescriptionConfig config) const = 0;
};

}  // namespace internal
}  // namespace cereal
