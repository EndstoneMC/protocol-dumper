#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/Context.h

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <entt/entt.hpp>

#include "common/EnableNonOwnerReferences.h"

namespace cereal {

enum class JSONSchemaOutput : bool {
    Exclude = false,
    Include = true,
};

namespace internal {

struct ReflectionContext {
    entt::meta_ctx mMetaCtx;
    std::vector<std::tuple<std::string, entt::type_info, JSONSchemaOutput>> mKnownProperties;
    std::optional<JSONSchemaOutput> mForcedJSONSchemaOutput;
};

}  // namespace internal

struct ReflectionCtx : private internal::ReflectionContext, Bedrock::EnableNonOwnerReferences {
    ReflectionContext &internal() { return static_cast<ReflectionContext &>(*this); }
    [[nodiscard]] const ReflectionContext &internal() const { return static_cast<const ReflectionContext &>(*this); }
};

}  // namespace cereal

template <typename T, typename Tag>
struct TypeWrapper : T {};
