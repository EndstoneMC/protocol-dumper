#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

struct Config {
    std::unordered_map<std::string, std::string> signatures;
    std::unordered_map<std::string, uintptr_t> offsets;

    // Load from a JSON file. Returns false and logs on failure.
    bool load(const std::filesystem::path &path);

    // Signature lookup — returns empty string if not found.
    const std::string &sig(const std::string &name) const;

    // Offset lookup — returns 0 if not found.
    uintptr_t offset(const std::string &name) const;
};
