#pragma once
// Minimal ABI-compatible Packet base class for BDS.
// Just enough to call createPacket(), getId(), and getName().
// Layout verified against Endstone's BEDROCK_STATIC_ASSERT_SIZE checks.

#include <cstdint>
#include <memory>
#include <string_view>

class BinaryStream;  // opaque — we never use it

class Packet {
public:
    virtual ~Packet() = default;
    virtual int getId() const = 0;
    virtual std::string_view getName() const = 0;
    virtual std::size_t getMaxSize() const { return 0; }
    virtual void checkSize_stub() {}
    virtual void writeWithSerializationMode_stub() {}
    virtual void write_with_ctx_stub() {}
    virtual void write(BinaryStream &) const {}
    virtual void read_with_ctx_stub() {}
    virtual void read_stub() {}
    virtual bool disallowBatching() const { return false; }
    virtual bool isValid() const { return true; }
    virtual int getSerializationMode() const { return 0; }
    virtual void setSerializationMode(int) {}
    virtual std::string toString() const { return ""; }

private:
    virtual void _read_with_ctx_stub() {}
    virtual void _read_stub() {}

protected:
    int priority_{2};
    int reliability_{1};
    uint8_t sender_sub_id_{0};
    bool is_handled_{false};
    char pad1_[6]{};
    int64_t recv_timepoint_{0};

private:
    const void *handler_{nullptr};
    int compressible_{0};
    char pad2_[4]{};
};
static_assert(sizeof(Packet) == 48);

// Factory function — resolved at runtime via RVA
using CreatePacketFn = std::shared_ptr<Packet> (*)(int);
