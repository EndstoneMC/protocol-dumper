#pragma once

#include "models.h"

namespace proto {

/**
 * Abstract visitor-style writer for protocol model nodes.
 *
 * Each `write` overload dispatches on the model node type. Call contract:
 *  - `write(Packet)` and `write(TypeDef)` are top-level entry points; each
 *    emits a complete artifact (implementations typically open a file or
 *    document, recursively populate it via the leaf overloads, and flush).
 *  - `write(Field)` and `write(EnumEntry)` append a single element to the
 *    implementation's current array-like scope (the enclosing top-level
 *    call is responsible for establishing that scope).
 *  - `write(TypeSpec)` and `write(Constraints)` attach keyed data to the
 *    implementation's current object-like scope (the enclosing `write(Field)`).
 */
class Writer {
public:
    virtual ~Writer() = default;

    virtual void write(const Packet &) = 0;
    virtual void write(const TypeDef &) = 0;

    virtual void write(const Field &) = 0;
    virtual void write(const EnumEntry &) = 0;
    virtual void write(const Constraints &) = 0;
    virtual void write(const TypeSpec &) = 0;
};

}  // namespace proto
