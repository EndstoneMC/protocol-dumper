#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "models.h"

namespace proto::output {

class JsonWriter {
public:
    void write(const std::vector<model::Packet> &packets,
               const std::vector<std::unique_ptr<model::Class>> &classes,
               const std::filesystem::path &output_dir) const;
};

}  // namespace proto::output
