#pragma once

#include <chrono>

class Connector {
public:
    struct ConnectionCallbacks {
        virtual ~ConnectionCallbacks() = default;
    };
};

class RakNetConnector {
public:
    struct ConnectionCallbacks : Connector::ConnectionCallbacks {};
};

class RakPeerHelper {
public:
    class IPSupportInterface {
    public:
        virtual ~IPSupportInterface();
    };
};

enum class Compressibility : int;

class NetworkPeer {
public:
    using PacketRecvTimepoint = std::chrono::steady_clock::time_point;
    using PacketRecvTimepointPtr = std::shared_ptr<PacketRecvTimepoint>;
    enum class Reliability : int;
};
