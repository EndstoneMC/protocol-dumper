#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "writer.h"

namespace proto {

class JsonWriter : public Writer {
public:
    explicit JsonWriter(std::filesystem::path output_dir);

    void write(const Packet &) override;
    void write(const TypeDef &) override;
    void write(const Field &) override;
    void write(const EnumEntry &) override;
    void write(const Constraints &) override;
    void write(const TypeSpec &) override;

private:
    using json = nlohmann::ordered_json;

    std::filesystem::path output_dir_;
    std::vector<json *> scope_;
    std::set<std::string> emitted_types_;
};

}  // namespace proto