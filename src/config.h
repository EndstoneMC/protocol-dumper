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
    std::vector<int> offsets;  // dereference chain after pattern match
    int extra = 0;             // added to final address
    bool relative = false;     // resolve as RIP-relative
};

struct OffsetEntry {
    std::string name;
    uintptr_t value = 0;
};

struct Config {
    std::string executable;
    std::vector<SignatureEntry> signatures;
    std::vector<OffsetEntry> offsets;

    bool load(const std::filesystem::path &path);

    // Lookup helpers — return nullptr/0 if not found
    const SignatureEntry *findSignature(const std::string &name) const;
    uintptr_t findOffset(const std::string &name) const;

    // Resolve a signature to a function address in the target process.
    // Handles pattern scan, offset chain dereference, extra, and RIP-relative.
    void *resolve(const std::string &name) const;
};