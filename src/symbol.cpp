#include "symbol.h"
#include "config.h"

#include <libhat.hpp>
#include <spdlog/spdlog.h>

void *resolveSymbol(const std::string &decorated_name)
{
    const auto *entry = Config::instance().findSignature(decorated_name);
    if (!entry) {
        spdlog::error("Symbol not in config: {}", decorated_name);
        return nullptr;
    }

    if (entry->pattern.empty()) {
        spdlog::error("Empty pattern for: {}", decorated_name);
        return nullptr;
    }

    auto sig = hat::parse_signature(entry->pattern);
    if (!sig) {
        spdlog::error("Invalid pattern for '{}': {}", decorated_name, entry->pattern);
        return nullptr;
    }

    auto mod = hat::process::get_process_module();
    auto result = hat::find_pattern(mod.get(), *sig);
    if (!result.has_result()) {
        spdlog::error("Pattern not found for: {}", decorated_name);
        return nullptr;
    }

    auto addr = reinterpret_cast<uintptr_t>(result.get());

    for (int off : entry->offsets) {
        addr = *reinterpret_cast<uintptr_t *>(addr + off);
    }

    if (entry->relative) {
        auto rip_offset = *reinterpret_cast<int32_t *>(addr);
        addr = addr + 4 + rip_offset;
    }

    addr += entry->extra;

    spdlog::info("Resolved '{}' @ 0x{:X}", decorated_name, addr);
    return reinterpret_cast<void *>(addr);
}
