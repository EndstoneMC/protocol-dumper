#pragma once

#include <libhat.hpp>
#include <mutex>
#include <shared_mutex>

#include "../symbol.h"
#include "NonOwnerPointer.h"
#include "PrioritySharedMutex.h"
#include "ServerInstance.h"

template <typename T>
class ServiceReference {
public:
    ServiceReference(Bedrock::NonOwnerPointer<T> service,
                     Bedrock::Threading::SharedLock<
                         Bedrock::Threading::PrioritySharedMutex<Bedrock::Threading::PrioritizeSharedOwnership>> &&lock)
        : mLock(std::move(lock)), mService(service)
    {
    }
    ServiceReference(Bedrock::NonOwnerPointer<T> service) : mService(service) {}
    void reset();
    T *operator->() const { return &*mService; }
    Bedrock::NonOwnerPointer<T> get() const { return mService; }
    operator bool() const;
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    bool operator==(const ServiceReference &) const;

private:
    Bedrock::Threading::SharedLock<
        Bedrock::Threading::PrioritySharedMutex<Bedrock::Threading::PrioritizeSharedOwnership>>
        mLock;
    Bedrock::NonOwnerPointer<T> mService;
};

template <typename T>
class ServiceLocator {
public:
    static ServiceReference<T> get();
};

template <>
inline ServiceReference<ServerInstance> ServiceLocator<ServerInstance>::get()
{
    static auto addr = []() -> std::byte * {
        auto signature = hat::compile_signature<"E8 ? ? ? ? 90 4C 8B 6D ? 4D 85 ED 0F 84">();
        auto result = hat::find_pattern(signature, ".text");
        if (!result.has_result()) {
            throw std::runtime_error("Failed to find pattern");
        }
        return result.rel(1);
    }();
    return BEDROCK_CALL(addr, &ServiceLocator::get);
}
