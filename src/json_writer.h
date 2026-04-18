#pragma once

#include <filesystem>
#include <vector>

#include "models.h"

namespace proto {

void write_json(const std::vector<Packet> &packets,
                const std::vector<TypeDef> &types,
                const std::filesystem::path &output_dir);

}  // namespace proto
