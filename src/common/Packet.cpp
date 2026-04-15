#include "Packet.h"

#include <libhat.hpp>

#include "../symbol.h"

std::shared_ptr<Packet> MinecraftPackets::createPacket(MinecraftPacketIds id)
{
    static auto addr = []() -> std::byte * {
        auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 4C 8B 7D ? 48 8B 4D ? E8">(), ".text");
        if (!result.has_result()) {
            throw std::runtime_error("Sigscan failed: MinecraftPackets::createPacket()");
        }
        return result.rel(1);
    }();
    return CALL_FUNCTION(addr, &MinecraftPackets::createPacket, id);
}
