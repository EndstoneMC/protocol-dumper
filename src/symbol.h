#pragma once

#include <cstddef>

/**
 * @file symbol.h
 * @brief Invoke raw code addresses as strongly-typed callables.
 */

/**
 * Reinterpret @p addr as a free-function pointer whose signature matches the
 * discarded prototype argument.
 */
template <typename Return, typename... Args>
Return (*fp_cast(Return (*)(Args...), void *addr))(Args...)
{
    return *reinterpret_cast<Return (**)(Args...)>(&addr);
}

/**
 * Reinterpret @p addr as a pointer-to-member-function.
 */
template <typename Return, typename Class, typename... Args>
Return (Class::*fp_cast(Return (Class::*)(Args...), void *addr))(Args...)
{
    struct member_function_pointer {
        void *ptr = nullptr;
        std::size_t adj = 0;
    } mfp{.ptr = addr};
    return *reinterpret_cast<Return (Class::**)(Args...)>(&mfp);
}

/**
 * `const` counterpart of the member-function overload. A separate overload is
 * needed because cv-qualifiers participate in pmf type deduction.
 */
template <typename Return, typename Class, typename... Args>
Return (Class::*fp_cast(Return (Class::*)(Args...) const, void *addr))(Args...) const
{
    struct member_function_pointer {
        void *ptr = nullptr;
        std::size_t adj = 0;
    } mfp{.ptr = addr};
    return *reinterpret_cast<Return (Class::**)(Args...) const>(&mfp);
}

/**
 * Invoke a raw code address through a typed prototype.
 *
 * `, ##__VA_ARGS__` swallows the comma when no extra args are passed, so
 * zero-arg calls are well-formed (GNU extension honoured by MSVC).
 *
 * @code
 *     auto packet = CALL_FUNCTION(addr, &MinecraftPackets::createPacket, id);
 * @endcode
 */
#define CALL_FUNCTION(addr, fp, ...) std::invoke(fp_cast(fp, addr), ##__VA_ARGS__)
