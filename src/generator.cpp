#include "generator.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

using cereal::SchemaDescription;
using cereal::internal::Member;
using cereal::internal::ReflectedType;

// ============================================================================
// Name helpers
// ============================================================================

std::string ProtoGenerator::sanitizeName(const std::string &raw)
{
    std::string result;
    result.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
            result += c;
        else if (c == ' ' || c == '-')
            result += '_';
    }
    if (result.empty()) result = "unknown";
    if (std::isdigit(static_cast<unsigned char>(result[0]))) result = "_" + result;
    return result;
}

std::string ProtoGenerator::toSnakeCase(const std::string &name)
{
    std::string s = sanitizeName(name);
    std::string result;
    result.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (std::isupper(static_cast<unsigned char>(c))) {
            if (i > 0 && (std::islower(static_cast<unsigned char>(s[i - 1])) ||
                          (i + 1 < s.size() &&
                           std::islower(static_cast<unsigned char>(s[i + 1])) &&
                           std::isupper(static_cast<unsigned char>(s[i - 1]))))) {
                result += '_';
            }
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        else {
            result += c;
        }
    }
    // Collapse multiple underscores, trim
    std::string collapsed;
    bool prev = false;
    for (char c : result) {
        if (c == '_') { if (!prev) collapsed += c; prev = true; }
        else { collapsed += c; prev = false; }
    }
    auto start = collapsed.find_first_not_of('_');
    auto end = collapsed.find_last_not_of('_');
    if (start == std::string::npos) { return "unknown"; }
    collapsed = collapsed.substr(start, end - start + 1);
    if (collapsed.empty()) collapsed = "unknown";
    return collapsed;
}

std::string ProtoGenerator::toPascalCase(const std::string &name)
{
    std::string snake = toSnakeCase(name);
    std::string result;
    bool cap = true;
    for (char c : snake) {
        if (c == '_') { cap = true; }
        else if (cap) { result += static_cast<char>(std::toupper(static_cast<unsigned char>(c))); cap = false; }
        else { result += c; }
    }
    if (result.empty()) result = "Unknown";
    return result;
}

std::string ProtoGenerator::toProtoFileName(const std::string &packet_name)
{
    return toSnakeCase(packet_name) + ".proto";
}

std::string ProtoGenerator::indent(int level)
{
    return std::string(level * 2, ' ');
}

// ============================================================================
// Type mapping
// ============================================================================

std::string ProtoGenerator::reflectedTypeToProto(ReflectedType type)
{
    switch (type) {
    case ReflectedType::Bool:   return "bool";
    case ReflectedType::Int8:   return "sint32";
    case ReflectedType::Uint8:  return "uint32";
    case ReflectedType::Int16:  return "sint32";
    case ReflectedType::Uint16: return "uint32";
    case ReflectedType::Int32:  return "sint32";
    case ReflectedType::Uint32: return "uint32";
    case ReflectedType::Int64:  return "sint64";
    case ReflectedType::Uint64: return "uint64";
    case ReflectedType::Float:  return "float";
    case ReflectedType::Double: return "double";
    case ReflectedType::String: return "string";
    default:                    return "bytes";
    }
}

// ============================================================================
// Constraint description
// ============================================================================

std::string ProtoGenerator::describeConstraints(const cereal::internal::ConstraintDescription &c)
{
    std::ostringstream ss;
    bool first = true;
    auto sep = [&] { if (!first) ss << ", "; first = false; };

    if (c.mMinimum || c.mMaximum) {
        sep();
        ss << "range: [";
        if (c.mMinimum) ss << *c.mMinimum; else ss << "-inf";
        ss << ", ";
        if (c.mMaximum) ss << *c.mMaximum; else ss << "inf";
        ss << "]";
    }
    if (c.mMinLength) { sep(); ss << "min_length: " << *c.mMinLength; }
    if (c.mMaxLength) { sep(); ss << "max_length: " << *c.mMaxLength; }
    if (c.mMinItems)  { sep(); ss << "min_items: " << *c.mMinItems; }
    if (c.mMaxItems)  { sep(); ss << "max_items: " << *c.mMaxItems; }
    if (c.mPattern)   { sep(); ss << "pattern: " << *c.mPattern; }
    return ss.str();
}

// ============================================================================
// Compound type collection (first pass)
// ============================================================================

void ProtoGenerator::collectCompoundTypes(const SchemaDescription &desc)
{
    if (!desc.mMembers) return;
    for (const auto &[name, member] : *desc.mMembers) {
        if (member.mType && *member.mType == ReflectedType::Object && member.mName) {
            compound_type_counts_[*member.mName]++;
            if (!compound_type_descs_.count(*member.mName)) {
                compound_type_descs_[*member.mName] = &member;
            }
            collectCompoundTypes(member);
        }
        // Recurse into container value types
        if (member.mValueType) collectCompoundTypes(*member.mValueType);
        if (member.mMappedType) collectCompoundTypes(*member.mMappedType);
    }
}

bool ProtoGenerator::isSharedType(const std::string &type_name) const
{
    return shared_types_.count(type_name) > 0;
}

// ============================================================================
// Generate
// ============================================================================

void ProtoGenerator::generate(const std::vector<PacketEntry> &packets,
                               const std::filesystem::path &output_dir)
{
    compound_type_counts_.clear();
    compound_type_descs_.clear();
    shared_types_.clear();

    for (const auto &pkt : packets) {
        collectCompoundTypes(pkt.schema);
    }
    for (const auto &[name, count] : compound_type_counts_) {
        if (count >= 2) shared_types_.insert(name);
    }

    emitCommonTypes(output_dir);

    for (const auto &pkt : packets) {
        emitPacketFile(pkt, output_dir);
    }
}

// ============================================================================
// common_types.proto
// ============================================================================

void ProtoGenerator::emitCommonTypes(const std::filesystem::path &output_dir)
{
    if (shared_types_.empty()) return;

    std::ofstream out(output_dir / "common_types.proto");
    out << "// Auto-generated shared types from BDS cereal schema dump\n";
    out << "syntax = \"proto3\";\n";
    out << "package bedrock.protocol;\n\n";

    for (const auto &type_name : shared_types_) {
        auto it = compound_type_descs_.find(type_name);
        if (it == compound_type_descs_.end()) continue;

        std::string msg_name = toPascalCase(type_name);
        emitMessage(out, msg_name, *it->second, 0);
        out << "\n";
    }
}

// ============================================================================
// Per-packet .proto file
// ============================================================================

void ProtoGenerator::emitPacketFile(const PacketEntry &packet,
                                     const std::filesystem::path &output_dir)
{
    std::string filename = toProtoFileName(packet.name);
    std::ofstream out(output_dir / filename);

    emitted_enums_.clear();

    out << "// Auto-generated from BDS cereal schema dump\n";
    out << "// Packet: " << packet.name << " (ID: " << packet.id << ")\n";
    out << "syntax = \"proto3\";\n";
    out << "package bedrock.protocol;\n\n";

    // Recursively check if any nested type references a shared type
    std::function<bool(const SchemaDescription &)> needsImport =
        [&](const SchemaDescription &desc) -> bool {
            if (!desc.mMembers) return false;
            for (const auto &[n, m] : *desc.mMembers) {
                if (m.mType && *m.mType == ReflectedType::Object &&
                    m.mName && isSharedType(*m.mName))
                    return true;
                if (m.mType && *m.mType == ReflectedType::Object && needsImport(m))
                    return true;
                if (m.mValueType && needsImport(*m.mValueType)) return true;
                if (m.mMappedType && needsImport(*m.mMappedType)) return true;
            }
            return false;
        };

    if (needsImport(packet.schema)) {
        out << "import \"common_types.proto\";\n\n";
    }

    std::string msg_name = toPascalCase(packet.name);
    emitMessage(out, msg_name, packet.schema, 0);
}

// ============================================================================
// Message emission
// ============================================================================

void ProtoGenerator::emitMessage(std::ostream &out, const std::string &msg_name,
                                  const SchemaDescription &desc, int ind)
{
    out << indent(ind) << "message " << msg_name << " {\n";

    if (desc.mMembers) {
        int field_num = 1;
        for (const auto &[name, member] : *desc.mMembers) {
            emitMember(out, name, member, field_num, ind + 1, msg_name);
        }
    }

    out << indent(ind) << "}\n";
}

// ============================================================================
// Member emission
// ============================================================================

void ProtoGenerator::emitMember(std::ostream &out, const std::string &field_name,
                                 const Member &member, int &field_num,
                                 int ind, const std::string &parent_msg_name)
{
    std::string snake = toSnakeCase(field_name);
    if (snake.empty()) snake = "field_" + std::to_string(field_num);

    ReflectedType rt = member.mType.value_or(ReflectedType::Null);

    switch (rt) {

    case ReflectedType::Object: {
        std::string type_name = member.mName.value_or(field_name);
        std::string msg = toPascalCase(type_name);

        if (isSharedType(type_name)) {
            out << indent(ind) << msg << " " << snake << " = " << field_num++ << ";\n";
        }
        else {
            emitMessage(out, msg, member, ind);
            out << indent(ind) << msg << " " << snake << " = " << field_num++ << ";\n";
        }
        break;
    }

    case ReflectedType::SequenceContainer: {
        if (member.mValueType) {
            ReflectedType elem_rt = member.mValueType->mType.value_or(ReflectedType::Null);
            if (elem_rt == ReflectedType::Object) {
                std::string elem_type = member.mValueType->mName.value_or(field_name + "Entry");
                std::string msg = toPascalCase(elem_type);
                if (!isSharedType(elem_type)) {
                    emitMessage(out, msg, *member.mValueType, ind);
                }
                out << indent(ind) << "repeated " << msg << " " << snake << " = " << field_num++ << ";\n";
            }
            else {
                std::string elem_proto = reflectedTypeToProto(elem_rt);
                out << indent(ind) << "repeated " << elem_proto << " " << snake << " = " << field_num++ << ";\n";
            }
        }
        else {
            out << indent(ind) << "repeated bytes " << snake << " = " << field_num++ << ";  // unknown element type\n";
        }
        break;
    }

    case ReflectedType::AssociativeContainer: {
        // proto3 map<K,V>
        std::string key_type = "string";
        std::string val_type = "bytes";
        if (member.mKeyType && member.mKeyType->mType)
            key_type = reflectedTypeToProto(*member.mKeyType->mType);
        if (member.mMappedType && member.mMappedType->mType) {
            if (*member.mMappedType->mType == ReflectedType::Object) {
                std::string obj_name = member.mMappedType->mName.value_or(field_name + "Value");
                val_type = toPascalCase(obj_name);
                if (!isSharedType(obj_name)) {
                    emitMessage(out, val_type, *member.mMappedType, ind);
                }
            }
            else {
                val_type = reflectedTypeToProto(*member.mMappedType->mType);
            }
        }
        out << indent(ind) << "map<" << key_type << ", " << val_type << "> "
            << snake << " = " << field_num++ << ";\n";
        break;
    }

    case ReflectedType::Enum: {
        // Emit enum definition if we have values
        std::string enum_name = member.mName.value_or(field_name);
        if (member.mEnumValues && !member.mEnumValues->empty()) {
            emitEnum(out, enum_name, *member.mEnumValues, ind);
        }
        // Use underlying type for the field
        std::string proto_type = "sint32";
        if (member.mUnderlyingType)
            proto_type = reflectedTypeToProto(*member.mUnderlyingType);

        std::string comment;
        if (!enum_name.empty()) comment = "  // enum: " + enum_name;
        if (member.mDeprecated) comment += "  // DEPRECATED";
        if (member.mConstraint) {
            auto cs = describeConstraints(*member.mConstraint);
            if (!cs.empty()) comment += "  // " + cs;
        }
        out << indent(ind) << proto_type << " " << snake << " = " << field_num++ << ";" << comment << "\n";
        break;
    }

    default: {
        // Primitive field
        std::string proto_type = reflectedTypeToProto(rt);
        std::string comment;
        if (member.mDeprecated) comment += "  // DEPRECATED";
        if (member.mRequired) comment += "  // required";
        if (member.mConstraint) {
            auto cs = describeConstraints(*member.mConstraint);
            if (!cs.empty()) comment += "  // " + cs;
        }
        if (member.mDescription) comment += "  // " + *member.mDescription;
        out << indent(ind) << proto_type << " " << snake << " = " << field_num++ << ";" << comment << "\n";
        break;
    }
    }
}

// ============================================================================
// Enum emission
// ============================================================================

void ProtoGenerator::emitEnum(std::ostream &out, const std::string &enum_name,
                               const std::vector<cereal::internal::EnumValue> &values,
                               int ind)
{
    std::string pascal = toPascalCase(enum_name);
    if (emitted_enums_.count(pascal)) return;
    emitted_enums_.insert(pascal);

    out << indent(ind) << "enum " << pascal << " {\n";

    bool has_zero = false;
    for (const auto &ev : values) {
        if (ev.mValue == 0) { has_zero = true; break; }
    }
    auto toUpper = [](std::string s) {
        for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    };
    std::string prefix = toUpper(toSnakeCase(enum_name));

    if (!has_zero) {
        out << indent(ind + 1) << prefix << "_UNSPECIFIED = 0;\n";
    }

    for (const auto &ev : values) {
        std::string full = prefix + "_" + toUpper(toSnakeCase(ev.mName));

        out << indent(ind + 1) << full << " = " << ev.mValue << ";";
        if (ev.mDescription && !ev.mDescription->empty()) {
            out << "  // " << *ev.mDescription;
        }
        out << "\n";
    }

    out << indent(ind) << "}\n";
}
