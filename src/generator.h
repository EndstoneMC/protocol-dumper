#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cereal/schema/SchemaDescription.h"

class SchemaGenerator {
public:
    struct PacketEntry {
        int id;
        std::string name;
        cereal::SchemaDescription schema;
    };

    void generate(const std::vector<PacketEntry> &packets, const std::filesystem::path &output_dir);

private:
    void collectCompoundTypes(const cereal::SchemaDescription &desc);

    nlohmann::ordered_json buildPacket(const PacketEntry &packet);
    nlohmann::ordered_json buildFields(const cereal::SchemaDescription &desc);
    nlohmann::ordered_json buildField(const std::string &field_name, const cereal::internal::Member &member);

    static std::string resolveWire(cereal::internal::ReflectedType type, cereal::SerializationTraits traits);
    static std::string reflectedTypeName(cereal::internal::ReflectedType type);
    static std::string sanitizeName(const std::string &raw);

    // Collect every unique compound type encountered during field building
    std::map<std::string, const cereal::SchemaDescription *> compound_types_;
};