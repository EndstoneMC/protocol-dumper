#include "Packet.h"

#include <libhat.hpp>

#include "../symbol.h"

std::shared_ptr<Packet> MinecraftPackets::createPacket(MinecraftPacketIds id)
{
    static auto addr = []() -> std::byte * {
        auto signature = hat::compile_signature<"E8 ? ? ? ? 90 48 83 BD ? ? ? ? ? 0F 84 ? ? ? ? FF 15">();
        auto result = hat::find_pattern(signature, ".text");
        if (!result.has_result()) {
            throw std::runtime_error("Failed to find pattern");
        }
        return result.rel(1);
    }();
    return BEDROCK_CALL(addr, &MinecraftPackets::createPacket, id);
}
