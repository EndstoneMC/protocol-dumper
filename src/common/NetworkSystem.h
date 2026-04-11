#pragma once

#include <vector>

struct WeakEntityRef {};

class NetworkSystem {
public:
    void update(const std::vector<WeakEntityRef> *userList);
};
