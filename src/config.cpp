#include "config.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

static const std::string empty_string;

bool Config::load(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Config file not found: {}", path.string());
        return false;
    }

    try {
        auto json = nlohmann::json::parse(file);

        if (json.contains("signatures")) {
            for (auto &[key, val] : json["signatures"].items()) {
                signatures[key] = val.get<std::string>();
            }
        }

        if (json.contains("offsets")) {
            for (auto &[key, val] : json["offsets"].items()) {
                offsets[key] = val.get<uintptr_t>();
            }
        }
    }
    catch (const nlohmann::json::exception &e) {
        spdlog::error("Failed to parse config: {}", e.what());
        return false;
    }

    spdlog::info("Loaded config: {} signatures, {} offsets",
                 signatures.size(), offsets.size());
    return true;
}

const std::string &Config::sig(const std::string &name) const
{
    auto it = signatures.find(name);
    if (it != signatures.end()) return it->second;
    spdlog::warn("Signature not found in config: {}", name);
    return empty_string;
}

uintptr_t Config::offset(const std::string &name) const
{
    auto it = offsets.find(name);
    if (it != offsets.end()) return it->second;
    spdlog::warn("Offset not found in config: {}", name);
    return 0;
}
