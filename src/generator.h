#pragma once

#include <filesystem>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "cereal/schema/SchemaDescription.h"

class ProtoGenerator {
public:
    struct PacketEntry {
        int id;
        std::string name;
        cereal::SchemaDescription schema;
    };

    void generate(const std::vector<PacketEntry> &packets, const std::filesystem::path &output_dir);

private:
    void collectCompoundTypes(const cereal::SchemaDescription &desc);
    bool isSharedType(const std::string &type_name) const;

    void emitCommonTypes(const std::filesystem::path &output_dir);
    void emitPacketFile(const PacketEntry &packet, const std::filesystem::path &output_dir);

    void emitMessage(std::ostream &out, const std::string &msg_name, const cereal::SchemaDescription &desc, int indent);
    void emitMember(std::ostream &out, const std::string &field_name, const cereal::internal::Member &member,
                    int &field_num, int indent, const std::string &parent_msg_name);
    void emitEnum(std::ostream &out, const std::string &enum_name,
                  const std::vector<cereal::internal::EnumValue> &values, int indent);

    static std::string reflectedTypeToProto(cereal::internal::ReflectedType type);
    static std::string sanitizeName(const std::string &raw);
    static std::string toSnakeCase(const std::string &name);
    static std::string toPascalCase(const std::string &name);
    static std::string toProtoFileName(const std::string &packet_name);
    static std::string indent(int level);
    static std::string describeConstraints(const cereal::internal::ConstraintDescription &c);

    std::map<std::string, int> compound_type_counts_;
    std::map<std::string, const cereal::SchemaDescription *> compound_type_descs_;
    std::set<std::string> shared_types_;
    std::set<std::string> emitted_enums_;
};
