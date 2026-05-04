#pragma once

#include "cereal/Context.h"

template <typename T, typename Tag>
struct TypeWrapper : T {
    TypeWrapper() = default;
    TypeWrapper(const T &value) : T(value) {}  // NOLINT(*-explicit-constructor)
};

namespace PacketSerialization {
constexpr auto CEREAL_PACKET_TAG = "[cereal:packet]";
constexpr auto CEREAL_PACKET_DETAILS_PROPERTY = "[cereal:packet_details]";
void bindPackets(cereal::ReflectionCtx &ctx);
}  // namespace PacketSerialization
