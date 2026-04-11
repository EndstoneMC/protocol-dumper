#include "config.h"

#include <fstream>

#include <libhat.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

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
                signatures.push_back(std::move(sig));
            }
        }

        if (json.contains("offsets")) {
            for (auto &entry : json["offsets"]) {
                OffsetEntry off;
                off.name = entry.value("name", "");
                off.value = entry.value("value", 0);
                offsets.push_back(std::move(off));
            }
        }
    }
    catch (const nlohmann::json::exception &e) {
        spdlog::error("Failed to parse config: {}", e.what());
        return false;
    }

    spdlog::info("Loaded config: {} signatures, {} offsets", signatures.size(), offsets.size());
    return true;
}

const SignatureEntry *Config::findSignature(const std::string &name) const
{
    for (const auto &sig : signatures) {
        if (sig.name == name) return &sig;
    }
    return nullptr;
}

uintptr_t Config::findOffset(const std::string &name) const
{
    for (const auto &off : offsets) {
        if (off.name == name) return off.value;
    }
    return 0;
}

void *Config::resolve(const std::string &name) const
{
    const auto *entry = findSignature(name);
    if (!entry) {
        spdlog::error("Signature '{}' not found in config", name);
        return nullptr;
    }

    if (entry->pattern.empty()) {
        spdlog::error("Signature '{}' has empty pattern", name);
        return nullptr;
    }

    // Parse the pattern string at runtime
    auto sig = hat::parse_signature(entry->pattern);
    if (!sig) {
        spdlog::error("Invalid pattern for '{}': {}", name, entry->pattern);
        return nullptr;
    }

    // Scan in the specified module
    auto mod = hat::process::get_process_module();
    auto result = hat::find_pattern(mod.get(), *sig);
    if (!result.has_result()) {
        spdlog::error("Pattern not found for '{}'", name);
        return nullptr;
    }

    auto addr = reinterpret_cast<uintptr_t>(result.get());

    // Follow offset dereference chain
    for (int off : entry->offsets) {
        addr = *reinterpret_cast<uintptr_t *>(addr + off);
    }

    // RIP-relative resolution: read 4-byte relative offset and resolve
    if (entry->relative) {
        auto rip_offset = *reinterpret_cast<int32_t *>(addr);
        addr = addr + 4 + rip_offset;  // RIP = addr + 4 (size of the offset itself)
    }

    // Apply extra offset
    addr += entry->extra;

    spdlog::info("Resolved '{}' @ 0x{:X}", name, addr);
    return reinterpret_cast<void *>(addr);
}