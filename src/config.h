#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct SignatureEntry {
    std::string name;
    std::string module;
    std::string pattern;
    std::vector<int> offsets;
    int extra = 0;
    bool relative = false;
};

struct Config {
    std::string executable;
    std::unordered_map<std::string, SignatureEntry> signatures;

    bool load(const std::filesystem::path &path);

    const SignatureEntry *findSignature(const std::string &name) const;

    static Config &instance();
};
