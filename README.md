# proto-dumper

Extracts packet schemas from a running Bedrock Dedicated Server (BDS) via cereal reflection and generates Protocol Buffer (`.proto`) definitions.

## How It Works

BDS uses an in-house serialization library called **cereal** (not to be confused with the open-source cereal library) built on top of [EnTT](https://github.com/skypjack/entt)'s meta reflection system. Every packet payload type is registered with cereal at startup, creating a complete type graph with field names, types, constraints, enum values, and more.

This tool injects a DLL into the BDS process, taps into that reflection data, and converts it to `.proto` files.

### Architecture

```
┌─────────────────────────────────┐
│  Type Registration              │  cerealizer<T>::bind() -> entt meta_ctx
├─────────────────────────────────┤
│  Schema Layer                   │  BasicSchema -> CompositeSchema/TypeSchema
│  (knows how to traverse types)  │  SchemaReader / SchemaWriter (abstract I/O)
├─────────────────────────────────┤
│  Format Layer                   │  BinarySchemaReader/Writer (BinaryStream)
│  (knows the wire format)        │  Could also be JSON, etc.
└─────────────────────────────────┘
```

### Pipeline

```
bedrock_server.exe (running)
       |
       |  DLL injection via injector.exe
       v
   DllMain -> CreateThread(DumpThread)
       |
       |-- libhat: sigscan for NetworkSystem::update
       |-- funchook: hook NetworkSystem::update
       |-- wait for first call -> capture NetworkSystem* this
       |-- unhook
       |
       |-- NetworkSystem::mReflectionCtx -> cereal::ReflectionCtx*
       |
       |-- libhat: sigscan for createPacket
       |-- createPacket(0, 1, 2, ...) -> enumerate packet IDs and names
       |         stop when factory returns nullptr, skip 200-299
       |
       |-- libhat: sigscan for BasicSchema::lookup, BasicSchema::description
       |-- entt::resolve(meta_ctx) -> iterate registered types
       |-- extract SchemaDescription per packet payload
       |
       v
  ProtoGenerator
       |
       |-- Pass 1: collect shared compound types (appear in 2+ packets)
       |-- Emit common_types.proto
       |-- Emit per-packet .proto files
       |
       v
  ./data/proto/
       |-- common_types.proto
       |-- move_player.proto
       |-- login.proto
       |-- ...
       |-- cereal_types.txt     (diagnostic: all registered entt types)
       |-- dump_log.txt         (processing log)
       |-- DONE.txt             (completion marker)
```

## Cereal Internals

### Type Registration

At BDS startup, every packet payload type calls `cerealizer<T>::bind(ctx)`:

```cpp
struct EmoteListPacketPayload {
    ActorRuntimeID mRuntimeId;
    std::vector<mce::UUID> mEmotePieceIds;
};

template<>
struct cerealizer<EmoteListPacketPayload> {
    static void bind(cereal::ReflectionCtx & ctx) {
        cereal::Factory<EmoteListPacketPayload> factory(
            ctx, "EmoteListPacketPayload", cereal::ReflectMode::Init);
        factory.bind<&EmoteListPacketPayload::mRuntimeId>("mRuntimeId")
               .bind<&EmoteListPacketPayload::mEmotePieceIds>("mEmotePieceIds");
    }
};
```

`bind<&T::mField>("name")` does the following:
1. Takes a pointer-to-member as a template parameter
2. Deduces the field type from the member pointer
3. Registers an `entt::meta_data` node with get/set function pointers
4. Stores field name, traits (required/optional/deprecated), and constraints
5. Recursively ensures the field's own type is also registered
6. Returns `Factory<T>&` for chaining

The Factory also supports:
- `.bindRequired<&T::mField>("name")` -- marks the field as required
- `.constraint(...)` -- adds min/max/pattern constraints
- `.deprecate("reason")` -- marks deprecated
- `.help("description")` -- adds field description
- `.serializationTraits(BigEndian)` -- per-field wire encoding hints

### Serialization Path

When a packet is sent, the write path is:

```
Packet::write(BinaryStream)
  -> PayloadSerializer::write<T>(packetId, ctx, payload, stream, mode)
       -> cerealWrite<T>(ctx, payload, stream)
            |
            +- BinarySchemaWriter writer(stream)    // wraps BinaryStream
            +- BasicSaver saver(ctx, config)        // orchestrator
            |
            +- saver.save<T>(writer, payload)
                 -> CompositeSchema<T>::doSave(writer, any)
                      // For each field registered via bind():
                      writer.pushMember("mRuntimeId")
                        TypeSchema<ActorRuntimeID>::doSave(writer, value)
                          writer.write(int64_t) -> stream.writeVarInt64()
                      writer.popMember()
```

`BinarySchemaWriter` translates abstract `write(int32_t)` calls into specific `BinaryStream` calls (`writeVarInt`, `writeSignedInt`, etc.) based on `SerializationTraits`.

The read path is the exact mirror using `BinarySchemaReader` and `BasicLoader`.

### Schema Description Extraction

`BasicSchema::description()` walks the same entt metadata but instead of reading/writing bytes, builds a `SchemaDescription` tree:

```cpp
SchemaDescription {
    .mType = Object,
    .mMembers = {
        "mRuntimeId": Member {
            .mType = Int64,
            .mRequired = false,
            .mDeprecated = false,
        },
        "mEmotePieceIds": Member {
            .mType = SequenceContainer,
            .mValueType = SchemaDescription { .mType = Object, ... },
        },
    }
}
```

This tree contains field names, types, constraints, full enum value tables, default values, and deprecation info -- everything needed to generate `.proto` definitions.

### Why Three RVAs

The three BDS functions we call are not exported, so we resolve them by offset (RVA) from the module base address:

| Function | Purpose |
|---|---|
| `ReflectionCtx::global()` | Get the singleton holding all registered types |
| `BasicSchema::lookup(meta_ctx, type_info)` | Find the schema object for a given type |
| `BasicSchema::description(ctx, config)` | Walk the schema and build the SchemaDescription tree |

The alternative (walking `entt::meta_type::data()` directly) gives member type IDs and get/set pointers, but **not** field names, constraints, enum value tables, or deprecation info. That metadata is packed into cereal-specific `meta_custom_node` data that only `description()` knows how to unpack.

### Serialization Modes

Each packet has a `SerializationMode`:

| Mode | Behavior |
|---|---|
| `ManualOnly` | Legacy hand-written `write(BinaryStream&)` only |
| `SideBySide_*` | Both manual and cereal, compare output for migration validation |
| `CerealOnly` | Pure cereal serialization |

BDS is actively migrating packets from manual to cereal. Since all packets have `cerealizer<T>::bind()` registrations regardless of mode, the reflection data is available for all of them.

### Proto Field Ordering

Proto field numbers don't exist in cereal. Fields are serialized **in registration order** (the order `bind()` is called). The generated `.proto` files assign sequential field numbers to match this order.

## Building

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Produces:
- `build/Release/proto_dumper.dll` -- the injected DLL
- `build/Release/injector.exe` -- the injector

## Usage

```bash
# Default: wait for bedrock_server.exe, inject proto_dumper.dll from same directory
injector.exe

# Custom process name
injector.exe -p my_server.exe

# Custom DLL path
injector.exe -d C:\path\to\proto_dumper.dll

# Timeout after 30 seconds
injector.exe -t 30
```

Output goes to `data/proto/` relative to the host executable.

## Configuration

### Signatures

All BDS function addresses are resolved at runtime via byte pattern scanning (libhat). Patterns are defined in `src/signatures.h`:

```cpp
namespace sig {
    constexpr auto NETWORK_SYSTEM_UPDATE = "?? ?? ?? ?? ??"_sig;  // TODO
    constexpr auto CREATE_PACKET         = "?? ?? ?? ?? ??"_sig;  // TODO
    constexpr auto BASIC_SCHEMA_LOOKUP   = "?? ?? ?? ?? ??"_sig;  // TODO
    constexpr auto BASIC_SCHEMA_DESC     = "?? ?? ?? ?? ??"_sig;  // TODO
}
```

To find a signature in IDA:
1. Navigate to the target function
2. Select the first ~15-20 bytes of the prologue
3. Copy as hex, replace variable bytes with `?`
4. Verify uniqueness (should match exactly once in the module)

Functions to find:
- `NetworkSystem::update` -- hooked to capture the NetworkSystem instance
- `MinecraftPackets::createPacket` -- called to enumerate packet IDs
- `cereal::internal::BasicSchema::lookup` -- resolves type to schema object
- `cereal::internal::BasicSchema::description` -- extracts SchemaDescription tree

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| [EnTT](https://github.com/skypjack/entt) | latest | Meta reflection (must match BDS's entt version for ABI compatibility) |
| [spdlog](https://github.com/gabime/spdlog) | v1.17.0 | Logging |
| [argparse](https://github.com/p-ranav/argparse) | v3.2 | CLI argument parsing for injector |
| [funchook](https://github.com/kubo/funchook) | v1.1.3 | Function hooking to capture NetworkSystem instance |
| [libhat](https://github.com/BasedInc/libhat) | latest | SIMD signature scanning for BDS functions |

All fetched automatically via CMake FetchContent.

## Project Structure

```
src/
    main.cpp                          DLL entry point, hook + sigscan orchestration
    generator.h/.cpp                  SchemaDescription -> .proto file generation
    reader.h/.cpp                     Cereal schema extraction from ReflectionCtx
    injector.cpp                      EXE that finds BDS process and injects the DLL
    signatures.h                      Byte patterns for all BDS functions
    common/
        Bedrock.h                     Bedrock::EnableNonOwnerReferences stub
        Packet.h                      Minimal ABI-compatible Packet base class
    cereal/                           ABI-compatible cereal type declarations
        Context.h                     ReflectionCtx, ReflectionContext
        schema/
            BasicSchema.h             BasicSchema base class
            DynamicValue.h            Variant-based JSON-like value type
            SchemaDescription.h       SchemaDescription, Member, EnumValue, constraints
            SerializationTraits.h     SerializationTraits, ContextArea enums
```

Files under `common/` and `cereal/` mirror BDS's own header layout for easy cross-referencing with [bedrock-headers](https://github.com/nicholass003/bedrock-headers-litelv).
