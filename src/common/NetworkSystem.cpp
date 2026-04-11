#include "NetworkSystem.h"

#include "../symbol.h"

void NetworkSystem::update(const std::vector<WeakEntityRef> *userList)
{
    BEDROCK_CALL(&NetworkSystem::update, this, userList);
}

const cereal::ReflectionCtx &NetworkSystem::getPacketReflectionCtx() const
{
    BEDROCK_CALL(&NetworkSystem::getPacketReflectionCtx, this);
}
