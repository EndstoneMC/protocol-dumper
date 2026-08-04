#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/BasicSchema.h
//
// Only the nested descriptor structs are consumed — read back from each entt
// meta node's custom data. BasicSchema's own methods are never called, so the
// class is reduced to what anchors those descriptors' layout.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "../Constraints.h"
#include "SerializationTraits.h"
#include "version.h"

namespace cereal {

namespace internal {

// entt user-defined traits on setter/getter meta-funcs (high 16 bits of the node
// traits). isDefaultSetter marks the readAndWriteAs primary setter (the binary
// read partner); bare alsoReadAs alternates lack it and are SkipAlsoReadAs-dropped.
// The same bits also land on meta_data nodes: isKeyedSetterGetter marks a member
// bound through a DynamicSetterArg thunk, which GenericCompositeSchema::doSave
// prefixes with a member-present bool.
enum class MemberTraits : uint16_t {
    noTraits = 0,
    isRequired = 1,
    isDefaultSetter = 2,
    isMemberLevelSetterGetter = 4,
    isKeyedSetterGetter = 8,
    isConstSelector = 16,
    hasDefaultValue = 32,
    _entt_enum_as_bitmask = 255,
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

    // Opaque to the dumper; only its size places the fields behind it.
    struct EnumMapping {
        struct View {};
        std::vector<View> mViews;
    };

    struct TypeDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::string mName;
#if BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 26, 30, 28)
        EnumMapping mEnumMapping;
#endif
        UserPropertiesMap mUserPropertiesMap;
        std::string mErrorMessage;
    };

    struct TaggedVariantDescriptor {
        DynamicSetterArgCtor mResolve;
        std::string mTaggedName;
    };

    struct SetterDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::unique_ptr<Constraint> mConstraint;
        ContextArea mAllowedAreas;
    };

    struct GetterDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        ContextArea mAllowedAreas;
    };

    struct MemberDescriptor {
        std::unique_ptr<BasicSchema> mPtr;
        std::unique_ptr<Constraint> mConstraint;
        std::string mOriginalEnumName;
        std::string mName;
        DynamicSetterArgCtor mDynamicSetterArgCtor;
        UserPropertiesMap mUserPropertiesMap;
        OverridingSet mOverridingTypes;
        std::string mErrorMessage;
        SerializationTraits mSerializationTraits;
        ContextArea mAllowedAreas;
#if BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 26, 30, 28)
        std::optional<unsigned int> mScope;
#endif
        bool mIsDeprecatedComponent;
    };

    virtual ~BasicSchema() = default;
};

}  // namespace internal
}  // namespace cereal
