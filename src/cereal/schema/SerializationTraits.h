#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/SerializationTraits.h
//          src-deps/Cereal/include/Cereal/SerializerContext.h (ContextArea)

#include <cstdint>

namespace cereal {

enum class ContextArea : uint8_t {
    PUBLIC = 1,
    VANILLA = 2,
    INTERNAL = 4,
    ALL = 255,
};

enum class SerializationTraits : uint8_t {
    None = 0,
    Compression = 1,
    BigEndian = 2,
    EnumAsValue = 4,
    NoSizeCompression = 8,
    SkipAlsoReadAs = 16,
};

}  // namespace cereal
