#pragma once

class ServerInstance : public Bedrock::EnableNonOwnerReferences,
                       public AppPlatformListener,
                       public GameCallbacks,
                       public Core::StorageAreaStateListener {};
