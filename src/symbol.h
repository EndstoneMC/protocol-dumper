#pragma once
// Type-safe BDS function call dispatch.
// Mirrors Endstone's BEDROCK_CALL pattern but resolves via runtime sigscan
// instead of a compile-time symbol table.
//
// Call resolveAllSymbols() once at startup to sigscan everything upfront.
// BEDROCK_CALL then just looks up the cached address.

#include <cstddef>
#include <functional>
#include <string>

// Resolve all signatures from Config in one pass. Returns false if any fail.
bool resolveAllSymbols();

// Look up a previously resolved symbol. Returns nullptr if not found.
void *getSymbol(const std::string &decorated_name);

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

// --- BEDROCK_CALL: invoke a BDS function by its cached decorated name ---

#define BEDROCK_CALL(fp, ...)                          \
    std::invoke(                                       \
        fp_cast(fp, getSymbol(__FUNCDNAME__)),          \
        ##__VA_ARGS__)
