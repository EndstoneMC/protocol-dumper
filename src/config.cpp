#include "config.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

static Config g_instance;

Config &Config::instance()
{
    return g_instance;
}

bool Config::load(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Config file not found: {}", path.string());
        return false;
    }

    try {
        auto json = nlohmann::json::parse(file);

        executable = json.value("executable", "bedrock_server.exe");

        if (json.contains("signatures")) {
            for (auto &entry : json["signatures"]) {
                SignatureEntry sig;
                sig.name = entry.value("name", "");
                sig.module = entry.value("module", executable);
                sig.pattern = entry.value("pattern", "");
                sig.extra = entry.value("extra", 0);
                sig.relative = entry.value("relative", false);
                if (entry.contains("offsets")) {
                    for (auto &off : entry["offsets"]) {
                        sig.offsets.push_back(off.get<int>());
                    }
                }
                signatures.emplace(sig.name, std::move(sig));
            }
        }
    }
    catch (const nlohmann::json::exception &e) {
        spdlog::error("Failed to parse config: {}", e.what());
        return false;
    }

    spdlog::info("Loaded config: {} signatures", signatures.size());
    return true;
}

const SignatureEntry *Config::findSignature(const std::string &name) const
{
    auto it = signatures.find(name);
    return it != signatures.end() ? &it->second : nullptr;
}