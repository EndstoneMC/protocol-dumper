#pragma once

#include "models.h"
#include "visitor.h"

namespace proto {

class Writer {
public:
    virtual ~Writer() = default;

    virtual void write(const Protocol &p)
    {
        for (const auto &pkt : p.packets) write(pkt);
        for (const auto &td : p.types) write(td);
    }

protected:
    virtual void write(const Packet &) = 0;
    virtual void write(const TypeDef &) = 0;
    virtual void write(const Field &) = 0;
    virtual void write(const EnumEntry &) = 0;
    virtual void write(const Constraints &) = 0;
    virtual void write(const TypeSpec &) = 0;
};

}  // namespace proto
