#pragma once

#include "NetworkSystem.h"
#include "Subscription.h"

class AppPlatformListener {
public:
    AppPlatformListener(bool doInit);
    virtual ~AppPlatformListener();

private:
    Bedrock::PubSub::Subscription mLowMemorySubscription;
    bool mListenerRegistered;
};

class GameCallbacks {
public:
    virtual ~GameCallbacks() = default;
};

namespace Core {
class StorageAreaStateListener {
public:
    StorageAreaStateListener();
    virtual ~StorageAreaStateListener() = default;

private:
    std::shared_ptr<void /*Core::FileStorageArea*/> mFileStorageArea;
    Bedrock::Threading::Mutex mMutex;
};
}  // namespace Core

class ServerInstance : public Bedrock::EnableNonOwnerReferences,
                       public AppPlatformListener,
                       public GameCallbacks,
                       public Core::StorageAreaStateListener {
public:
    ServerNetworkSystem &getNetwork() { return *mNetwork; }
    std::chrono::steady_clock::time_point mLastSyncTime;

private:
    const bool mIsDedicatedServer;
    std::unique_ptr<class Minecraft> mMinecraft;
    std::unique_ptr<ServerNetworkSystem> mNetwork;
};
