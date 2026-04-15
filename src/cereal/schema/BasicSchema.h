#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/BasicSchema.h
//
// lookup() and description() are implemented by us directly — no sigscan needed.
// lookup() reads the schema from the entt meta_type_node's custom data.
// description() delegates to virtual makeDescription() on the BDS-owned object.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "SchemaDescription.h"

namespace cereal {

class Constraint;  // forward decl; we only hold it by unique_ptr in descriptor mirrors
struct SchemaWriter;
struct SchemaReader;

namespace internal {

struct ReflectionContext;
struct LoadState;
struct SaveState;

enum class VariantPriorityLevel : uint8_t {
    UNKNOWN = 0,
    OTHER = 1,
    INTEGER = 2,
    FLOAT = 3,
    DOUBLE = 4,
};

enum class TypeTraits : uint16_t {
    noTraits = 0,
    isTaggedVariant = 1,
    hasTopLevelSetters = 2,
    hasTaggedVariantMembers = 4,
    hasDefaultMembers = 8,
};

enum class MemberTraits : uint16_t {
    noTraits = 0,
    isRequired = 1,
    isDefaultSetter = 2,
    isMemberLevelSetterGetter = 4,
    isKeyedSetterGetter = 8,
    isConstSelector = 16,
    hasDefaultValue = 32,
};

enum class OverrideType : int {
    deprecated = 0,
    overridden = 1,
};

struct OverrideState {
    entt::id_type mId;
    OverrideType mType;
};

using UserPropertiesMap =
    entt::dense_map<std::string, std::pair<entt::meta_type (*)(const entt::meta_ctx &), entt::basic_any<>>>;

class BasicSchema {
public:
    using IdType = entt::id_type;
    using DynamicSetterArgCtor = entt::meta_type (*)(const entt::meta_ctx &);
    // Stand-in for brstd::flat_set<OverrideState, std::less<void>, std::vector<OverrideState>>.
    // The flat_set specialization privately inherits an associative_adapter that holds only
    // the key container, so the layout is identical to std::vector<OverrideState>.
    using OverridingSet = std::vector<OverrideState>;

    struct TypeDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::string mName;
        UserPropertiesMap mUserPropertiesMap;
        std::string mErrorMessage;
    };

    // Mirrored ahead of use; consumed when the dumper starts walking tagged variants.
    struct TaggedVariantDescriptor {
        DynamicSetterArgCtor mResolve;
        std::string mTaggedName;
    };

    struct SetterDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::unique_ptr<cereal::Constraint> mConstraint;
        cereal::ContextArea mAllowedAreas;
    };

    struct GetterDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        cereal::ContextArea mAllowedAreas;
    };

    struct MemberDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::unique_ptr<cereal::Constraint> mConstraint;
        std::string mOriginalEnumName;
        std::string mName;
        DynamicSetterArgCtor mDynamicSetterArgCtor;
        UserPropertiesMap mUserPropertiesMap;
        OverridingSet mOverridingTypes;
        std::string mErrorMessage;
        cereal::SerializationTraits mSerializationTraits;
        cereal::ContextArea mAllowedAreas;
        bool mIsDeprecatedComponent;
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
    virtual void doLoad(SchemaReader &reader, entt::meta_any &any, const entt::meta_any &udata,
                        const LoadState &state) const
    {
    }
    virtual void doSave(SchemaWriter &writer, const entt::meta_any &any, const SaveState &state) const {}
    virtual void unknown1() {}  // TODO: figure out the actual function signature
    virtual void unknown2() {}
    [[nodiscard]] virtual SchemaDescription makeDescription(const ReflectionContext &ctx,
                                                            DescriptionConfig config) const = 0;
};

}  // namespace internal
}  // namespace cereal
