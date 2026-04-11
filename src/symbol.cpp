#include "symbol.h"
#include "config.h"

#include <unordered_map>

#include <libhat.hpp>
#include <spdlog/spdlog.h>

static std::unordered_map<std::string, void *> g_resolved;

static void *scanSignature(const SignatureEntry &entry) {
    if (entry.pattern.empty()) {
        spdlog::error("Empty pattern for: {}", entry.name);
        return nullptr;
    }

    auto sig = hat::parse_signature(entry.pattern);
    if (!sig.has_value()) {
        spdlog::error("Invalid pattern for '{}': {}", entry.name, entry.pattern);
        return nullptr;
    }

    auto result = hat::find_pattern(sig.value(), ".text");
    if (!result.has_result()) {
        spdlog::error("Pattern not found for: {}", entry.name);
        return nullptr;
    }

    auto addr = reinterpret_cast<uintptr_t>(result.get());

    for (int off: entry.offsets) {
        addr = *reinterpret_cast<uintptr_t *>(addr + off);
    }

    if (entry.relative) {
        auto rip_offset = *reinterpret_cast<int32_t *>(addr);
        addr = addr + 4 + rip_offset;
    }

    addr += entry.extra;
    return reinterpret_cast<void *>(addr);
}

bool resolveAllSymbols() {
    const auto &signatures = Config::instance().signatures;
    bool all_ok = true;

    for (const auto &[name, entry]: signatures) {
        auto *addr = scanSignature(entry);
        if (addr) {
            g_resolved[name] = addr;
            spdlog::info("Resolved '{}' @ 0x{:X}", name, reinterpret_cast<uintptr_t>(addr));
        } else {
            all_ok = false;
        }
    }

    spdlog::info("Resolved {}/{} symbols", g_resolved.size(), signatures.size());
    return all_ok;
}

void *getSymbol(const std::string &decorated_name) {
    auto it = g_resolved.find(decorated_name);
    if (it != g_resolved.end()) return it->second;
    spdlog::error("Symbol not resolved: {}", decorated_name);
    return nullptr;
}
