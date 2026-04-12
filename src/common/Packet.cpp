#include "Packet.h"

#include <libhat.hpp>

#include "../symbol.h"

std::shared_ptr<Packet> MinecraftPackets::createPacket(MinecraftPacketIds id)
{
    static auto addr = []() -> std::byte * {
        hat::scan_result result;
        if (g_bds_preview) {
            result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 90 48 83 BD ? ? ? ? ? 0F 84 ? ? ? ? FF 15">(), ".text");
        }
        else {
            result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 90 48 83 BD ? ? ? ? ? 0F 84 ? ? ? ? FF 15">(), ".text");
        }
        if (!result.has_result()) {
            throw std::runtime_error("Sigscan failed: MinecraftPackets::createPacket()");
        }
        return result.rel(1);
    }();
    return BEDROCK_CALL(addr, &MinecraftPackets::createPacket, id);
}
