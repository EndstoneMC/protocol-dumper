#pragma once
// Mirrors: src/common/network/NetworkSystem.h (minimal)

#include <vector>

// Forward declarations — we don't need full definitions
struct WeakEntityRef {};

namespace cereal {
struct ReflectionCtx;
}

class NetworkSystem {
public:
    void update(const std::vector<WeakEntityRef> *userList);
    const cereal::ReflectionCtx &getPacketReflectionCtx() const;
};
