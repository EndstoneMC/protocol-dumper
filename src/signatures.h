#pragma once
// Byte patterns for BDS functions resolved via libhat signature scanning.
// These are version-specific. Update from IDA/Ghidra when BDS updates.
//
// To find a pattern in IDA:
//   1. Navigate to the function
//   2. Select the first ~15-20 bytes of the function prologue
//   3. Copy as "Hex string", replace variable bytes with '?'
//   4. Test uniqueness: Search > Sequence of bytes — should match exactly once

#include <libhat/Scanner.hpp>

namespace sig {

using namespace hat::literals;

// NetworkSystem::update(const std::vector<WeakEntityRef>*)
// Hooked to capture the NetworkSystem* this pointer.
constexpr auto NETWORK_SYSTEM_UPDATE = "?? ?? ?? ?? ??"_sig;  // TODO

// MinecraftPackets::createPacket(MinecraftPacketIds) -> shared_ptr<Packet>
constexpr auto CREATE_PACKET = "?? ?? ?? ?? ??"_sig;  // TODO

// cereal::internal::BasicSchema::lookup(const entt::meta_ctx&, entt::type_info)
constexpr auto BASIC_SCHEMA_LOOKUP = "?? ?? ?? ?? ??"_sig;  // TODO

// cereal::internal::BasicSchema::description(const ReflectionContext&, DescriptionConfig) const
constexpr auto BASIC_SCHEMA_DESC = "?? ?? ?? ?? ??"_sig;  // TODO

}  // namespace sig