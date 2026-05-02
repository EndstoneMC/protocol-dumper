# protocol-dumper

Dumps Bedrock Dedicated Server packet schemas as JSON.

`protocol_dumper.dll` is injected into a running BDS process, walks the cereal/EnTT reflection graph the server builds at startup, and writes one JSON file per packet, struct, and enum.

## Build

clang-cl + lld-link, to match the toolchain Mojang now uses for BDS preview. From a VS 2022 x64 Native Tools prompt:

```
cmake --preset clang-cl-release
cmake --build --preset clang-cl-release
```

Use `clang-cl-relwithdebinfo` for a build with debug info. Outputs `build/release/protocol_dumper.dll` and `build/release/injector.exe`.

## Run

```
injector.exe                   # waits for bedrock_server.exe, injects the sibling DLL
injector.exe -p name.exe       # different target process
injector.exe -d path\to.dll    # different DLL
injector.exe -t 30             # process-wait timeout
injector.exe --dll-timeout 10  # DLL-finish timeout (default 5s)
```

Schemas land in `data/protocol/` next to the host executable.

## Schema

Kaitai-flavoured JSON. Each field has a wire `type` plus optional modifiers (`enum`, `repeat`, `optional`, `deprecated`, `constraints`, `description`). Maps are `{key, value}` objects; variants are `{switch, cases}`.

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

EnTT (must match the version BDS was built against — ABI), nlohmann/json, argparse, libhat, expected-lite. All fetched via CMake FetchContent.
