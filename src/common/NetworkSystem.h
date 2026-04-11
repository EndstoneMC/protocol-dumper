#pragma once

#include "Async.h"
#include "BinaryStream.h"
#include "NetworkPeer.h"
#include "NonOwnerPointer.h"
#include "Packet.h"
#include "PrioritySharedMutex.h"
#include "cereal/Context.h"

class AppPlatform;
enum class DevConnectionQuality : int;
enum class PacketCompressionAlgorithm : uint16_t;

struct NetworkSettingOptions {
    NetworkSettingOptions();
    NetworkSettingOptions(uint16_t, PacketCompressionAlgorithm, std::byte, float, bool, bool);
    uint16_t mCompressionThreshold;
    PacketCompressionAlgorithm mCompressionAlgorithm;
    bool mClientThrottleEnabled;
    uint8_t mClientThrottleThreshold;
    float mClientThrottleScalar;
    bool mRaknetJoinFloodProtectionEnabled;
    bool mEncryptionDisabled;
    DevConnectionQuality mDevConnectionQuality;
};

class NetworkEnableDisableListener {
    enum class State : int;

public:
    NetworkEnableDisableListener(const Bedrock::NonOwnerPointer<AppPlatform> &appPlatform);
    NetworkEnableDisableListener() = delete;
    virtual ~NetworkEnableDisableListener() = default;

private:
    State mState;
    Bedrock::NonOwnerPointer<AppPlatform> mAppPlatform;
};

class NetworkSystem : public RakNetConnector::ConnectionCallbacks,
                      public RakPeerHelper::IPSupportInterface,
                      public NetworkEnableDisableListener {
public:
    [[nodiscard]] const cereal::ReflectionCtx &getPacketReflectionCtx() const { return *mReflectionCtx; }

protected:
    Bedrock::NonOwnerPointer<class NetworkSessionOwner> mNetworkSessionOwner;
    Bedrock::Threading::RecursiveMutex mConnectionsMutex;
    std::vector<std::unique_ptr<class NetworkConnection>> mConnections;
    std::unique_ptr<class LocalConnector> mLocalConnector;
    std::shared_ptr<void /*PacketGroupDefinition::PacketGroupBuilder*/> mPacketGroupBuilder;

private:
    std::unique_ptr<class RemoteConnector> mRemoteConnector;
    std::unique_ptr<class ServerLocator> mServerLocator;
    size_t mCurrentConnection;
    Bedrock::Threading::Async<void> mReceiveTask;
    std::unique_ptr<class TaskGroup> mReceiveTaskGroup;
    Bedrock::NonOwnerPointer<class IPacketObserver> mPacketObserver;
    class Scheduler &mMainThread;
    std::string mReceiveBuffer;
    std::string mSendBuffer;
    BinaryStream mSendStream;
    struct IncomingPacketQueue {
        class NetEventCallback &callback_obj;
        Bedrock::Threading::Mutex mutex;
    };
    std::unique_ptr<IncomingPacketQueue> mIncomingPackets[4];
    bool mUseIPv6Only;
    uint16_t mDefaultGamePort;
    uint16_t mDefaultGamePortv6;
    bool mIsLanDiscoveryEnabled;
    std::unique_ptr<class NetworkStatistics> mNetworkStatistics;
    bool mWebsocketsEnabled;
    NetworkSettingOptions mNetworkSettingOptions;
    bool mRawRecordingEnabled;
    std::unique_ptr<cereal::ReflectionCtx> mReflectionCtx;
};

class ServerNetworkSystem : public Bedrock::EnableNonOwnerReferences, public NetworkSystem {};
