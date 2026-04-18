# Schema Redesign (Kaitai-Inspired)

## Why Not Proto2/Proto3?

The project previously used proto3 output (commit `8a4240c`) and moved to JSON because proto types imply proto's own wire encoding. Proto2 has the same problem: 11 of 26 Bedrock wire types have no proto2 equivalent (all 8-bit, 16-bit, and big-endian types). Structural features like length-prefixed arrays, string-encoded enums, and switch-on variants also can't be expressed. Custom proto options would make the `.proto` files misleading — the actual encoding is hidden in annotations that standard tooling ignores.

Kaitai Struct describes binary formats but only generates parsers (not writers) and lacks native varint/map support.

**Decision**: Codegen-ready JSON as the intermediate format. Consumers build codegen tools on top.

---

## Design Justification (Kaitai Struct parallels)

Each design choice is justified by a direct parallel in Kaitai Struct's KSY format.

### 1. `type` is the wire encoding — not a structural wrapper

In Kaitai, `type` names exactly what's on the wire. It's a plain string for scalars and named types.

```yaml
# Kaitai KSY
seq:
  - id: x
    type: f4le            # 4-byte little-endian float — wire type
  - id: target
    type: actor_runtime_id  # named sub-type — wire type by reference
```

```json
// Our JSON
{ "name": "X", "type": "float_le" }
{ "name": "Target Runtime ID", "type": "ActorRuntimeID" }
```

In our old format, `type` was overloaded: `"type": "array"` and `"type": "variant"` used it as a structural discriminator, not a wire type. Kaitai never does this — compound structures are expressed through modifiers (`repeat`, `switch-on`), not by hijacking `type`.

### 2. `enum` is a field annotation that overlays the wire type

In Kaitai, `enum` is a separate key that maps the raw integer to named values. The field's `type` is always the wire encoding; `enum` is layered on top.

```yaml
# Kaitai KSY
seq:
  - id: protocol
    type: u1              # wire type = unsigned 1-byte int
    enum: ip_protocol     # overlay: maps 6 -> "tcp", 17 -> "udp", etc.
```

```json
// Our JSON
{ "name": "Event ID", "type": "uint8", "enum": "ActorEvent" }
```

Our old format put enums in a separate type kind (`"enum": "ActorEvent", "type": "uint8"`) where the `enum` key appeared before `type` and consumers had to check for it first. The Kaitai pattern keeps `type` as the primary key and `enum` as an annotation — simpler to parse and consistent with the principle that `type` = wire encoding.

### 3. `repeat` makes any field into a collection

In Kaitai, repetition is orthogonal to type. Any field can become an array by adding `repeat` + `repeat-expr`. The `type` still describes one element.

```yaml
# Kaitai KSY
seq:
  - id: num_items
    type: u4le
  - id: items
    type: item_record     # type = one element
    repeat: expr
    repeat-expr: num_items # how many
```

```json
// Our JSON (count prefix baked into repeat value)
{ "name": "Items", "type": "ItemData", "repeat": "uvarint32" }
```

Kaitai separates count field from repeated field. Bedrock always uses inline length-prefixed arrays, so we collapse the count into `repeat`'s value — it names the prefix wire type directly. The composability is the same: `type` describes one element, `repeat` says how many.

This also works for maps. Kaitai has no native map type — it uses arrays of key-value pair types. We express the pair inline:

```json
// Map = repeated key-value pair
{ "name": "Biomes", "type": { "key": "uint16_le", "value": "BiomeDefinitionData" }, "repeat": "uvarint32" }
```

### 4. `switch-on` / `cases` for discriminated unions

In Kaitai, polymorphic types use `switch-on` to reference a discriminator field, and `cases` to map values to types.

```yaml
# Kaitai KSY
seq:
  - id: rec_type
    type: u1
    enum: record_type
  - id: body
    type:
      switch-on: rec_type
      cases:
        'record_type::header': header_body
        'record_type::data': data_body
```

```json
// Our JSON — tagged variant
{
  "name": "Body",
  "type": {
    "switch-on": "Message Type",
    "cases": [
      "TextPacketPayload::MessageOnly",
      "TextPacketPayload::AuthorAndMessage",
      "TextPacketPayload::MessageAndParams"
    ]
  }
}
```

Our old format used `"type": "variant"` with a separate `"tag"` object — an ad-hoc representation that consumers wouldn't recognize. The `switch-on` pattern is well-known from Kaitai and immediately communicates intent: "read the discriminator field, then parse one of these types."

### 5. Modifier composability

Kaitai's field keys are orthogonal modifiers: `type`, `enum`, `repeat`, `if` can all appear on the same field. Our format follows the same principle:

```yaml
# Kaitai KSY — repeated enum field
seq:
  - id: flags
    type: u1
    enum: flag_type
    repeat: expr
    repeat-expr: num_flags
```

```json
// Our JSON — array of enums
{ "name": "Flags", "type": "uint8", "enum": "FlagType", "repeat": "uvarint32" }
```

Each modifier is independent: `type` = wire encoding, `enum` = name mapping, `repeat` = collection. A codegen tool processes them in any order.

---

## Context

The output format needs finalizing for codegen consumers. The core problem is that field types are **flattened** into field objects via `j.update(mType->toJson())` in `models.cpp:82`. Secondary issues: variant alternatives leak C++ names, struct types lack explicit `kind`, untagged variants have no index encoding, anonymous namespaces leak.

The redesign draws from **Kaitai Struct** principles:
- `type` always names what's on the wire (string = wire type or type reference)
- `enum` is an annotation on a field, not a type kind
- `repeat` makes any field into an array (orthogonal to type)
- `switch-on` / `cases` for discriminated unions

## Field Schema

```
name         : string              (required)
type         : string | object     (required) — see Type System below
enum         : string              (optional) — enum name, overlays the wire type
repeat       : string              (optional) — prefix wire type, makes this field an array/map
optional     : bool                (optional, absent = required)
deprecated   : bool                (optional, absent = false)
description  : string              (optional)
constraints  : object              (optional) — min/max/length/pattern/items
```

## Type System

`type` is **string | object**:

### String (simple types — ~80% of fields)
```json
"type": "varint32"        // wire type
"type": "Vec3"            // named type reference
```
Consumer checks the known wire type set to disambiguate.

### Object — map entry (has `key` + `value`)
```json
"type": { "key": "uint16_le", "value": "BiomeDefinitionData" }
```
Always paired with `repeat` at field level.

### Object — variant (has `cases`)
```json
"type": {
  "switch-on": "Message Type",
  "cases": ["TypeA", "TypeB", "TypeC"]
}
```

## `repeat` — arrays and maps

Any field with `repeat` is a length-prefixed collection. The `repeat` value is the wire type of the count prefix.

**Array:** `type` is a string (element type), `repeat` gives the count prefix:
```json
{ "name": "Items", "type": "ItemData", "repeat": "uvarint32" }
```

**Array of scalars:**
```json
{ "name": "Bytes", "type": "uint8", "repeat": "uvarint32" }
```

**Array of enums:**
```json
{ "name": "Flags", "type": "uint8", "enum": "FlagType", "repeat": "uvarint32" }
```

**Map:** `type` is a `{key, value}` object, `repeat` gives the count prefix:
```json
{ "name": "Biomes", "type": { "key": "uint16_le", "value": "BiomeDefinitionData" }, "repeat": "uvarint32" }
```

The modifiers compose: `type` says what one element looks like, `repeat` says how many, `enum` overlays a name mapping.

## `enum` — Kaitai-style annotation

Enums stay flat on the field. `type` is the wire encoding, `enum` names the enum:
```json
{ "name": "Event ID", "type": "uint8", "enum": "ActorEvent" }
{ "name": "Permission", "type": "string", "enum": "CommandPermissionLevel" }
```

Unchanged from current format.

## Variants — `switch-on` / `cases`

Inspired by Kaitai's `switch-on` pattern.

### Tagged variant (discriminator is a sibling field)
```json
{
  "name": "Body",
  "type": {
    "switch-on": "Message Type",
    "cases": [
      "TextPacketPayload::MessageOnly",
      "TextPacketPayload::AuthorAndMessage",
      "TextPacketPayload::MessageAndParams"
    ]
  }
}
```

`switch-on` is the name of the sibling field that discriminates. The consumer looks up that field to find its `enum` and wire encoding. `cases` lists the alternative types (positional — index corresponds to enum value ordering).

### Untagged variant (inline index)
```json
{
  "name": "Rule Value",
  "type": {
    "switch-on": "<prefix_encoding>",
    "cases": ["null", "bool", "int32_le", "float_le"]
  }
}
```

When `switch-on` is a wire type (not a field name), it's an inline index — read the index using that encoding, then deserialize the corresponding case. Needs investigation in cereal to determine the prefix encoding.

### Variant alternative types
- C++ primitives resolved to wire types (`int` -> `int32_le`, `float` -> `float_le`, etc.)
- `cereal::NullType` -> `"null"` (zero bytes)
- `std::basic_string<char>` -> `"string"`
- Named types kept as-is (`"BookEditAction::ReplacePage"`)

## Type Definitions

### Struct (add `kind`)
```json
{
  "name": "Vec3",
  "kind": "struct",
  "fields": [
    { "name": "X", "type": "float_le" },
    { "name": "Y", "type": "float_le" },
    { "name": "Z", "type": "float_le" }
  ]
}
```

### Enum (unchanged)
```json
{
  "name": "ActorEvent",
  "kind": "enum",
  "values": [
    { "name": "NONE", "value": 0 },
    { "name": "JUMP", "value": 1 }
  ]
}
```

### Packet (unchanged structure, fields use new format)
```json
{
  "id": 27,
  "name": "ActorEventPacket",
  "fields": [
    { "name": "Target Runtime ID", "type": "ActorRuntimeID" },
    { "name": "Event ID", "type": "uint8", "enum": "ActorEvent" },
    { "name": "Data", "type": "varint32" },
    { "name": "Fire At Position", "type": "Vec3", "optional": true }
  ]
}
```

## Full Before/After Examples

### Array field
```
BEFORE: { "name": "Enum Values", "type": "array", "prefix": "uvarint32", "element": { "type": "string" } }
AFTER:  { "name": "Enum Values", "type": "string", "repeat": "uvarint32" }
```

### Array with constraints
```
BEFORE: { "name": "Data", "type": "array", "prefix": "uvarint32", "element": { "type": "CommandData" }, "constraints": { "max_items": 250 } }
AFTER:  { "name": "Data", "type": "CommandData", "repeat": "uvarint32", "constraints": { "max_items": 250 } }
```

### Map field
```
BEFORE: { "name": "Biomes", "type": "map", "prefix": "uvarint32", "key": { "type": "uint16_le" }, "value": { "type": "BiomeDefinitionData" } }
AFTER:  { "name": "Biomes", "type": { "key": "uint16_le", "value": "BiomeDefinitionData" }, "repeat": "uvarint32" }
```

### Tagged variant
```
BEFORE: { "name": "Body", "type": "variant", "tag": { "name": "Message Type", "type": "TextPacketType" }, "values": [...] }
AFTER:  { "name": "Body", "type": { "switch-on": "Message Type", "cases": [...] } }
```

### Untagged variant (with resolved wire types)
```
BEFORE: { "name": "Rule Value", "type": "variant", "values": ["cereal::NullType", "bool", "int", "float"] }
AFTER:  { "name": "Rule Value", "type": { "switch-on": "...", "cases": ["null", "bool", "int32_le", "float_le"] } }
```

### Enum field (unchanged)
```json
{ "name": "Event ID", "type": "uint8", "enum": "ActorEvent" }
```

### Full packet example (ActorEventPacket)
```json
{
  "id": 27,
  "name": "ActorEventPacket",
  "fields": [
    { "name": "Target Runtime ID", "type": "ActorRuntimeID" },
    { "name": "Event ID", "type": "uint8", "enum": "ActorEvent" },
    { "name": "Data", "type": "varint32" },
    { "name": "Fire At Position", "type": "Vec3", "optional": true }
  ]
}
```

### Full packet example (AvailableCommandsPacket)
```json
{
  "id": 76,
  "name": "AvailableCommandsPacket",
  "fields": [
    { "name": "Enum Values", "type": "string", "repeat": "uvarint32" },
    { "name": "Chained Subcommand Values", "type": "string", "repeat": "uvarint32" },
    { "name": "Post Fixes", "type": "string", "repeat": "uvarint32" },
    { "name": "Enum Data", "type": "AvailableCommandsPacketPayload::EnumData", "repeat": "uvarint32" },
    {
      "name": "Chained Subcommand Data",
      "type": "AvailableCommandsPacketPayload::ChainedSubcommandData",
      "repeat": "uvarint32",
      "constraints": { "max_items": 16 }
    },
    { "name": "Commands", "type": "AvailableCommandsPacketPayload::CommandData", "repeat": "uvarint32" },
    { "name": "Soft Enums", "type": "AvailableCommandsPacketPayload::SoftEnumData", "repeat": "uvarint32" },
    { "name": "Constraints", "type": "AvailableCommandsPacketPayload::ConstrainedValueData", "repeat": "uvarint32" }
  ]
}
```

### Full packet example (TextPacket)
```json
{
  "id": 9,
  "name": "TextPacket",
  "fields": [
    { "name": "Localize?", "type": "bool" },
    {
      "name": "Body",
      "type": {
        "switch-on": "Message Type",
        "cases": [
          "TextPacketPayload::MessageOnly",
          "TextPacketPayload::AuthorAndMessage",
          "TextPacketPayload::MessageAndParams"
        ]
      }
    },
    { "name": "Sender's XUID", "type": "string" },
    { "name": "Platform Id", "type": "string" },
    { "name": "Filtered Message", "type": "string", "optional": true }
  ]
}
```

## Code Changes

### 1. `src/models.h` — Restructure model classes

**Remove `ArrayFieldType` and `MapFieldType` as FieldType subclasses.** Arrays and maps are now field-level modifiers (`repeat`, `key`/`value`), not type kinds. The field carries repeat info directly.

**`Field` struct gains:**
```cpp
std::string mRepeat;  // prefix wire type, empty = not repeated
```

**Remove `EnumFieldType`**. Enum info moves to `Field`:
```cpp
std::optional<std::string> mEnum;  // enum name (field annotation)
```
The wire type goes through the normal `ScalarFieldType`.

**Revised FieldType hierarchy:**
- `ScalarFieldType` — wire type string, `toJson()` returns `Json(mWire)`
- `ObjectFieldType` — named type reference string, `toJson()` returns `Json(mTypeName)`
- `MapEntryFieldType` — `{key, value}` pair, `toJson()` returns `{"key": ..., "value": ...}`
- `VariantFieldType` — `{switch-on, cases}`, `toJson()` returns `{"switch-on": ..., "cases": [...]}`

**`Field::toJson()`:**
```cpp
Json j = Json::object();
j["name"] = mName;
if (mType) j["type"] = mType->toJson();
if (mEnum) j["enum"] = *mEnum;
if (!mRepeat.empty()) j["repeat"] = mRepeat;
if (!mRequired) j["optional"] = true;
if (mDeprecated) j["deprecated"] = true;
if (mDescription) j["description"] = *mDescription;
if (mConstraints && !mConstraints->empty()) j["constraints"] = mConstraints->toJson();
```

### 2. `src/models.cpp` — Update toJson() implementations

- `ScalarFieldType::toJson()`: returns `Json(mWire)` (plain string)
- `ObjectFieldType::toJson()`: returns `Json(mTypeName)` (plain string)
- `MapEntryFieldType::toJson()`: returns `{"key": mKey->toJson(), "value": mValue->toJson()}`
- `VariantFieldType::toJson()`: returns `{"switch-on": mSwitchOn, "cases": mCases}`
- `Class::toJson()`: add `j["kind"] = "struct"` for struct types

### 3. `src/visitor.cpp` — Adapt to new model

- `visitEnum()`: Create `ScalarFieldType` with wire type. Set `mEnum` on the field.
- `visitArray()`: Visit element type into `mTypeSlot`, set `mRepeat = lengthWire(traits)` on field.
- `visitMap()`: Create `MapEntryFieldType`, set `mRepeat = lengthWire(traits)` on field.
- `buildVariant()`: Populate `mSwitchOn` + `mCases`. Resolve C++ primitives to wire types. `cereal::NullType` -> `"null"`, `std::basic_string<char>` -> `"string"`.
- Strip `(anonymous namespace)::` from type names.

### 4. `src/main.cpp` — Share `stripTypePrefix`

Move duplicate `stripTypePrefix` to shared `src/util.h`.

## Open Investigations (cereal)

1. **Variant index encoding**: For untagged `std::variant`, what encoding does cereal use for the discriminant index? `VariantPriorityLevel` in `BasicSchema.h` suggests cereal may use priority-based disambiguation for some variants.

2. **Variant alternative wire types**: For primitive alternatives (`bool`, `int`, `float`), resolve wire encoding from cereal reflection metadata.

3. **`ConstraintDescription.mVariantTypes`**: Per-alternative constraint vector exists but isn't extracted. Consider emitting per-alternative constraints.

## Verification

1. Build the project after changes
2. Inject into BDS and dump protocol data
3. Validate:
   - No field has top-level `prefix`, `element`, `tag` keys
   - All struct type defs have `"kind": "struct"`
   - Arrays use `"repeat"` at field level
   - Maps use `"type": {"key": ..., "value": ...}` + `"repeat"`
   - Variants use `"type": {"switch-on": ..., "cases": [...]}`
   - Enum fields have `"type": "<wire>"` + `"enum": "<name>"`
   - No `cereal::NullType`, `std::basic_string<char>`, or `(anonymous namespace)` in output
