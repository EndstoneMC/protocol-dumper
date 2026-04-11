#include "Packet.h"

#include "../symbol.h"

std::shared_ptr<Packet> MinecraftPackets::createPacket(int id)
{
    return BEDROCK_CALL(&MinecraftPackets::createPacket, id);
}
