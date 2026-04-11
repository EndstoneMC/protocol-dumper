#pragma once
// Minimal ABI-compatible Packet base class for BDS.
// Just enough to call createPacket(), getId(), and getName().
// Layout verified against Endstone's BEDROCK_STATIC_ASSERT_SIZE checks.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "BinaryStream.h"
#include "NetworkPeer.h"
#include "Result.h"
#include "cereal/Context.h"

enum class MinecraftPacketIds : int;

enum class SerializationMode : int {
    ManualOnly = 0,
    SideBySide_LogOnMismatch = 1,
    SideBySide_AssertOnMismatch = 2,
    SemanticSideBySide_LogOnMismatch = 3,
    SemanticSideBySide_AssertOnMismatch = 4,
    CerealOnly = 5,
};
enum PacketPriority : unsigned int;
enum SubClientId : uint8_t;

class Packet {
public:
    virtual ~Packet() = default;
    virtual MinecraftPacketIds getId() const = 0;
    virtual std::string_view getName() const = 0;
    virtual size_t getMaxSize() const;
    virtual Bedrock::Result<void> checkSize(size_t packetSize, bool receiverIsServer) const;
    virtual void writeWithSerializationMode(BinaryStream &bitStream, const cereal::ReflectionCtx &reflectionCtx,
                                            std::optional<SerializationMode>) const;
    virtual void write(BinaryStream &bitStream, const cereal::ReflectionCtx &reflectionCtx) const;
    virtual void write(BinaryStream &) const = 0;
    virtual Bedrock::Result<void> read(ReadOnlyBinaryStream &bitStream, const cereal::ReflectionCtx &reflectionCtx);
    virtual Bedrock::Result<void> read(ReadOnlyBinaryStream &bitStream);
    virtual bool disallowBatching() const;
    virtual bool isValid() const;
    virtual SerializationMode getSerializationMode() const;
    virtual void setSerializationMode(SerializationMode);
    virtual std::string toString() const;

protected:
    PacketPriority mPriority;
    NetworkPeer::Reliability mReliability;
    SubClientId mSenderSubId;
    bool mIsHandled;
    NetworkPeer::PacketRecvTimepoint mReceiveTimepoint;

private:
    virtual Bedrock::Result<void> _read(ReadOnlyBinaryStream &bitStream, const cereal::ReflectionCtx &reflectionCtx);
    virtual Bedrock::Result<void> _read(ReadOnlyBinaryStream &) = 0;
    const class IPacketHandlerDispatcher *mHandler;
    Compressibility mCompressible;
};

class MinecraftPackets {
public:
    static std::shared_ptr<Packet> createPacket(MinecraftPacketIds id);
};
