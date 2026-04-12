#pragma once

inline bool g_bds_preview = false;

template <typename Return, typename... Args>
Return (*fp_cast(Return (*)(Args...), void *addr))(Args...)
{
    return *reinterpret_cast<Return (**)(Args...)>(&addr);
}

template <typename Return, typename Class, typename... Args>
Return (Class::*fp_cast(Return (Class::*)(Args...), void *addr))(Args...)
{
    struct MFP {
        void *ptr;
        std::size_t adj = 0;
    } temp{addr};
    return *reinterpret_cast<Return (Class::**)(Args...)>(&temp);
}

template <typename Return, typename Class, typename... Args>
Return (Class::*fp_cast(Return (Class::*)(Args...) const, void *addr))(Args...) const
{
    struct MFP {
        void *ptr;
        std::size_t adj = 0;
    } temp{addr};
    return *reinterpret_cast<Return (Class::**)(Args...) const>(&temp);
}

// --- BEDROCK_CALL: invoke a BDS function by its cached decorated name ---

#define BEDROCK_CALL(addr, fp, ...) std::invoke(fp_cast(fp, addr), ##__VA_ARGS__)
