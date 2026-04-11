#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/Context.h

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <entt/entt.hpp>

#include "../common/Bedrock.h"

namespace cereal {

enum class JSONSchemaOutput : bool {
    Exclude = 0,
    Include = 1,
};

namespace internal {

struct ReflectionContext {
    entt::meta_ctx mMetaCtx;
    std::vector<std::tuple<std::string, entt::type_info, JSONSchemaOutput>> mKnownProperties;
    std::optional<JSONSchemaOutput> mForcedJSONSchemaOutput;
};

}  // namespace internal

struct ReflectionCtx : private internal::ReflectionContext, Bedrock::EnableNonOwnerReferences {
    internal::ReflectionContext &internal() {
        return static_cast<internal::ReflectionContext &>(*this);
    }
    const internal::ReflectionContext &internal() const {
        return static_cast<const internal::ReflectionContext &>(*this);
    }
};

}  // namespace cereal
