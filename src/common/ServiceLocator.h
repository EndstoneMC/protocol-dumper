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
        auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 48 8B 45 ? 48 85 C0 74 ? ? ? ? 84 DB">(), ".text");
        if (!result.has_result()) {
            throw std::runtime_error("Sigscan failed: ServiceLocator<ServerInstance>::get()");
        }
        return result.rel(1);
    }();
    return CALL_FUNCTION(addr, &ServiceLocator::get);
}
