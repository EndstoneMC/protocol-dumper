#pragma once
// Type-safe BDS function call dispatch.
// Mirrors Endstone's BEDROCK_CALL pattern but resolves via runtime sigscan
// instead of a compile-time symbol table.
//
// Usage: define BDS class methods in headers, implement in .cpp with BEDROCK_CALL.
// __FUNCDNAME__ (MSVC) produces the decorated symbol name that matches config.json.

#include <cstddef>
#include <functional>
#include <string>

// --- Symbol resolution (implemented in symbol.cpp) ---

void *resolveSymbol(const std::string &decorated_name);

// --- fp_cast: convert between function pointers and void* ---

template <typename Return, typename... Args>
Return (*fp_cast(Return (*)(Args...), void *addr))(Args...)
{
    return *reinterpret_cast<Return (**)(Args...)>(&addr);
}

template <typename Return, typename Class, typename... Args>
Return (Class::*fp_cast(Return (Class::*)(Args...), void *addr))(Args...)
{
    struct MFP { void *ptr; std::size_t adj = 0; } temp{addr};
    return *reinterpret_cast<Return (Class::**)(Args...)>(&temp);
}

template <typename Return, typename Class, typename... Args>
Return (Class::*fp_cast(Return (Class::*)(Args...) const, void *addr))(Args...) const
{
    struct MFP { void *ptr; std::size_t adj = 0; } temp{addr};
    return *reinterpret_cast<Return (Class::**)(Args...) const>(&temp);
}

// --- BEDROCK_CALL: resolve and invoke a BDS function by decorated name ---

#define BEDROCK_CALL(fp, ...)                              \
    std::invoke(                                           \
        fp_cast(fp, resolveSymbol(__FUNCDNAME__)),         \
        ##__VA_ARGS__)
