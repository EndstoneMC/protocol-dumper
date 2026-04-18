#pragma once

#include <filesystem>
#include <vector>

#include "models.h"

namespace proto::output {

void write_json(const std::vector<model::Packet> &packets,
                const std::vector<model::TypeDef> &types,
                const std::filesystem::path &output_dir);

}  // namespace proto::output
