#include "generator.h"

#include <algorithm>
#include <fstream>

using cereal::SerializationTraits;
using cereal::SchemaDescription;
using cereal::internal::Member;
using cereal::internal::ReflectedType;
using json = nlohmann::ordered_json;

// ============================================================================
// Helpers
// ============================================================================

static bool hasFlag(SerializationTraits traits, SerializationTraits flag)
{
    return (static_cast<uint8_t>(traits) & static_cast<uint8_t>(flag)) != 0;
}

static SerializationTraits getTraits(const SchemaDescription &desc)
{
    return desc.mSerializationTraits.value_or(SerializationTraits::None);
}

std::string SchemaGenerator::sanitizeName(const std::string &raw)
{
    std::string result;
    result.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            result += c;
        }
        else if (c == ' ' || c == '-') {
            result += '_';
        }
    }
    if (result.empty()) {
        result = "unknown";
    }
    return result;
}

// ============================================================================
// Wire type resolution
// ============================================================================

std::string SchemaGenerator::reflectedTypeName(ReflectedType type)
{
    switch (type) {
    case ReflectedType::Bool:   return "bool";
    case ReflectedType::Int8:   return "int8";
    case ReflectedType::Uint8:  return "uint8";
    case ReflectedType::Int16:  return "int16";
    case ReflectedType::Uint16: return "uint16";
    case ReflectedType::Int32:  return "int32";
    case ReflectedType::Uint32: return "uint32";
    case ReflectedType::Int64:  return "int64";
    case ReflectedType::Uint64: return "uint64";
    case ReflectedType::Float:  return "float";
    case ReflectedType::Double: return "double";
    case ReflectedType::String: return "string";
    case ReflectedType::Enum:   return "enum";
    case ReflectedType::SequenceContainer:    return "array";
    case ReflectedType::AssociativeContainer: return "map";
    case ReflectedType::Object: return "object";
    default: return "unknown";
    }
}

std::string SchemaGenerator::resolveWire(ReflectedType type, SerializationTraits traits)
{
    bool compress = hasFlag(traits, SerializationTraits::Compression);
    bool big_end  = hasFlag(traits, SerializationTraits::BigEndian);

    switch (type) {
    case ReflectedType::Bool:   return "bool";
    case ReflectedType::Float:  return "f32_le";
    case ReflectedType::Double: return "f64_le";
    case ReflectedType::String: return "string";

    case ReflectedType::Int8:
        return compress ? "zigzag32" : "i8";
    case ReflectedType::Uint8:
        return compress ? "varint32" : "u8";

    case ReflectedType::Int16:
        if (compress) return "zigzag32";
        return big_end ? "i16_be" : "i16_le";
    case ReflectedType::Uint16:
        if (compress) return "varint32";
        return big_end ? "u16_be" : "u16_le";

    case ReflectedType::Int32:
        if (compress) return "zigzag32";
        return big_end ? "i32_be" : "i32_le";
    case ReflectedType::Uint32:
        if (compress) return "varint32";
        return big_end ? "u32_be" : "u32_le";

    case ReflectedType::Int64:
        if (compress) return "zigzag64";
        return big_end ? "i64_be" : "i64_le";
    case ReflectedType::Uint64:
        if (compress) return "varint64";
        return big_end ? "u64_be" : "u64_le";

    default:
        return "unknown";
    }
}

// ============================================================================
// Constraint description
// ============================================================================

static json buildConstraints(const cereal::internal::ConstraintDescription &c)
{
    json j = json::object();
    if (c.mMinimum)    j["minimum"] = *c.mMinimum;
    if (c.mMaximum)    j["maximum"] = *c.mMaximum;
    if (c.mMinLength)  j["min_length"] = *c.mMinLength;
    if (c.mMaxLength)  j["max_length"] = *c.mMaxLength;
    if (c.mMinItems)   j["min_items"] = *c.mMinItems;
    if (c.mMaxItems)   j["max_items"] = *c.mMaxItems;
    if (c.mPattern)    j["pattern"] = *c.mPattern;
    return j;
}

// ============================================================================
// Compound type collection
// ============================================================================

void SchemaGenerator::collectCompoundTypes(const SchemaDescription &desc)
{
    if (!desc.mMembers) return;
    for (const auto &[name, member] : *desc.mMembers) {
        if (member.mType && *member.mType == ReflectedType::Object && member.mName) {
            if (!compound_types_.count(*member.mName)) {
                compound_types_[*member.mName] = &member;
            }
            collectCompoundTypes(member);
        }
        if (member.mValueType) {
            if (member.mValueType->mType && *member.mValueType->mType == ReflectedType::Object && member.mValueType->mName) {
                if (!compound_types_.count(*member.mValueType->mName)) {
                    compound_types_[*member.mValueType->mName] = member.mValueType.get();
                }
            }
            collectCompoundTypes(*member.mValueType);
        }
        if (member.mMappedType) {
            if (member.mMappedType->mType && *member.mMappedType->mType == ReflectedType::Object && member.mMappedType->mName) {
                if (!compound_types_.count(*member.mMappedType->mName)) {
                    compound_types_[*member.mMappedType->mName] = member.mMappedType.get();
                }
            }
            collectCompoundTypes(*member.mMappedType);
        }
    }
}

// ============================================================================
// Field building
// ============================================================================

json SchemaGenerator::buildField(const std::string &field_name, const Member &member)
{
    json field = json::object();
    field["name"] = field_name;

    if (member.mOrdinalIndex) {
        field["ordinal"] = *member.mOrdinalIndex;
    }

    ReflectedType rt = member.mType.value_or(ReflectedType::Null);
    SerializationTraits traits = getTraits(member);

    switch (rt) {

    case ReflectedType::Enum: {
        field["type"] = "enum";
        if (member.mName) {
            field["type_name"] = *member.mName;
        }

        bool enum_as_value = hasFlag(traits, SerializationTraits::EnumAsValue);
        if (enum_as_value && member.mUnderlyingType) {
            auto remaining = static_cast<SerializationTraits>(
                static_cast<uint8_t>(traits) & ~static_cast<uint8_t>(SerializationTraits::EnumAsValue));
            field["wire"] = resolveWire(*member.mUnderlyingType, remaining);
        }
        else {
            field["wire"] = "string";
        }

        if (member.mUnderlyingType) {
            field["underlying_type"] = reflectedTypeName(*member.mUnderlyingType);
        }

        if (member.mEnumValues && !member.mEnumValues->empty()) {
            json values = json::array();
            for (const auto &ev : *member.mEnumValues) {
                json v = json::object();
                v["name"] = ev.mName;
                v["value"] = ev.mValue;
                if (ev.mDescription && !ev.mDescription->empty()) {
                    v["description"] = *ev.mDescription;
                }
                values.push_back(std::move(v));
            }
            field["enum_values"] = std::move(values);
        }
        break;
    }

    case ReflectedType::Object: {
        std::string type_name = member.mName.value_or(field_name);
        field["type"] = "object";
        field["type_name"] = type_name;
        field["fields"] = buildFields(member);
        break;
    }

    case ReflectedType::SequenceContainer: {
        field["type"] = "array";

        bool no_size_compress = hasFlag(traits, SerializationTraits::NoSizeCompression);
        field["length_wire"] = no_size_compress ? "u32_le" : "varint32";

        if (member.mValueType) {
            ReflectedType elem_rt = member.mValueType->mType.value_or(ReflectedType::Null);
            if (elem_rt == ReflectedType::Object) {
                std::string elem_name = member.mValueType->mName.value_or(field_name + "Entry");
                json elem = json::object();
                elem["type"] = "object";
                elem["type_name"] = elem_name;
                elem["fields"] = buildFields(*member.mValueType);
                field["element"] = std::move(elem);
            }
            else if (elem_rt == ReflectedType::Enum) {
                json elem = buildField("element", static_cast<const Member &>(*member.mValueType));
                elem.erase("name");
                elem.erase("ordinal");
                field["element"] = std::move(elem);
            }
            else {
                SerializationTraits elem_traits = getTraits(*member.mValueType);
                json elem = json::object();
                elem["type"] = reflectedTypeName(elem_rt);
                elem["wire"] = resolveWire(elem_rt, elem_traits);
                field["element"] = std::move(elem);
            }
        }
        break;
    }

    case ReflectedType::AssociativeContainer: {
        field["type"] = "map";

        bool no_size_compress = hasFlag(traits, SerializationTraits::NoSizeCompression);
        field["length_wire"] = no_size_compress ? "u32_le" : "varint32";

        if (member.mKeyType && member.mKeyType->mType) {
            SerializationTraits key_traits = getTraits(*member.mKeyType);
            json key = json::object();
            key["type"] = reflectedTypeName(*member.mKeyType->mType);
            key["wire"] = resolveWire(*member.mKeyType->mType, key_traits);
            field["key"] = std::move(key);
        }

        if (member.mMappedType && member.mMappedType->mType) {
            ReflectedType val_rt = *member.mMappedType->mType;
            if (val_rt == ReflectedType::Object) {
                std::string obj_name = member.mMappedType->mName.value_or(field_name + "Value");
                json val = json::object();
                val["type"] = "object";
                val["type_name"] = obj_name;
                val["fields"] = buildFields(*member.mMappedType);
                field["value"] = std::move(val);
            }
            else {
                SerializationTraits val_traits = getTraits(*member.mMappedType);
                json val = json::object();
                val["type"] = reflectedTypeName(val_rt);
                val["wire"] = resolveWire(val_rt, val_traits);
                field["value"] = std::move(val);
            }
        }
        break;
    }

    default: {
        field["type"] = reflectedTypeName(rt);
        field["wire"] = resolveWire(rt, traits);
        break;
    }
    }

    if (member.mRequired) {
        field["required"] = true;
    }
    if (member.mDeprecated) {
        field["deprecated"] = true;
    }
    if (member.mDescription) {
        field["description"] = *member.mDescription;
    }
    if (member.mConstraint) {
        auto c = buildConstraints(*member.mConstraint);
        if (!c.empty()) {
            field["constraints"] = std::move(c);
        }
    }

    return field;
}

// ============================================================================
// Build sorted field list from SchemaDescription
// ============================================================================

json SchemaGenerator::buildFields(const SchemaDescription &desc)
{
    if (!desc.mMembers) {
        return json::array();
    }

    struct FieldEntry {
        std::string name;
        const Member *member;
        unsigned char ordinal;
    };

    std::vector<FieldEntry> entries;
    entries.reserve(desc.mMembers->size());
    for (const auto &[name, member] : *desc.mMembers) {
        entries.push_back({name, &member, member.mOrdinalIndex.value_or(255)});
    }
    std::sort(entries.begin(), entries.end(), [](const FieldEntry &a, const FieldEntry &b) {
        return a.ordinal < b.ordinal;
    });

    json fields = json::array();
    for (const auto &entry : entries) {
        fields.push_back(buildField(entry.name, *entry.member));
    }
    return fields;
}

// ============================================================================
// Build packet JSON
// ============================================================================

json SchemaGenerator::buildPacket(const PacketEntry &packet)
{
    json j = json::object();
    j["name"] = packet.name;
    j["id"] = packet.id;
    j["fields"] = buildFields(packet.schema);
    return j;
}

// ============================================================================
// Generate
// ============================================================================

void SchemaGenerator::generate(const std::vector<PacketEntry> &packets, const std::filesystem::path &output_dir)
{
    compound_types_.clear();

    auto packets_dir = output_dir / "packets";
    auto types_dir = output_dir / "types";
    std::filesystem::create_directories(packets_dir);
    std::filesystem::create_directories(types_dir);

    // Collect all compound types while building packet JSON
    for (const auto &pkt : packets) {
        collectCompoundTypes(pkt.schema);
    }

    // Emit per-packet files
    for (const auto &pkt : packets) {
        std::string filename = sanitizeName(pkt.name) + ".json";
        std::ofstream out(packets_dir / filename);
        out << buildPacket(pkt).dump(2) << "\n";
    }

    // Emit per-type files
    for (const auto &[type_name, desc] : compound_types_) {
        json j = json::object();
        j["name"] = type_name;
        j["fields"] = buildFields(*desc);

        std::string filename = sanitizeName(type_name) + ".json";
        std::ofstream out(types_dir / filename);
        out << j.dump(2) << "\n";
    }
}