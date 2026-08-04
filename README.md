# protocol-dumper

Dumps Bedrock Dedicated Server packet schemas as JSON.

`libprotocol_dumper.so` is `LD_PRELOAD`ed into BDS, walks the cereal/EnTT reflection graph the server builds at startup, and writes one JSON file per packet, struct, and enum.

## Build

clang++ + libc++ + lld:

```
cmake --preset clang-release
cmake --build --preset clang-release
```

Use `clang-relwithdebinfo` for a build with debug info. Outputs `build/release/libprotocol_dumper.so`.

## Run

```
LD_PRELOAD=./build/release/libprotocol_dumper.so ./bedrock_server
```

Schemas land in `data/protocol/` next to the host executable.

## Schema

Kaitai-flavoured JSON. Each field has a wire `type` plus optional modifiers (`enum`, `repeat`, `optional`, `deprecated`, `constraints`, `description`, `value`). Maps are `{key, value}` objects; variants are `{switch, cases}`. A field with `value` is a nameless literal on the wire, Kaitai's `valid: <literal>`.

```json
{
  "id": 27,
  "name": "ActorEventPacket",
  "fields": [
    { "name": "mRuntimeId", "type": "varint64" },
    { "name": "mEventId",   "type": "uvarint32", "enum": "ActorEvent" }
  ]
}
```

## Dependencies

EnTT (must match the version BDS was built against — ABI), nlohmann/json, libhat, expected-lite. All fetched via CMake FetchContent.
