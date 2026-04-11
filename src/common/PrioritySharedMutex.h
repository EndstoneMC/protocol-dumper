#pragma once

namespace Bedrock::Threading {
using ConditionVariable = std::condition_variable;
using ConditionVariableAny = std::condition_variable_any;
#ifdef _WIN32
using Mutex = std::mutex;
using RecursiveMutex = std::recursive_mutex;
using SharedMutex = std::shared_mutex;
#endif

#ifdef __linux__
using Mutex = std::mutex;
using RecursiveMutex = std::recursive_mutex;
using SharedMutex = std::shared_timed_mutex;
#endif

template <typename T>
class UniqueLock : public std::unique_lock<T> {};
template <typename T>
class LockGuard : public std::lock_guard<T> {
    using std::lock_guard<T>::lock_guard;
};
template <typename T>
class SharedLock : public std::shared_lock<T> {};

template <typename LockingStrategy>
class PrioritySharedMutex {
public:
    PrioritySharedMutex() = default;
    PrioritySharedMutex(const PrioritySharedMutex &) = delete;
    ~PrioritySharedMutex() = default;
    PrioritySharedMutex &operator=(const PrioritySharedMutex &) = delete;
    void lock();
    bool try_lock();
    void unlock();
    void lock_shared();
    bool try_lock_shared();
    void unlock_shared();
    LockingStrategy &getStrategy() { return mStrategy; }

private:
    LockingStrategy mStrategy;
};

class PrioritizeSharedOwnership {
public:
    void lock_shared();
    bool try_lock_shared();
    void unlock_shared();
    void lock();
    bool try_lock();
    void unlock();
    size_t waitingWriters();

private:
    SharedMutex mMutex;
    ConditionVariableAny mZeroReaders;
    std::atomic<unsigned long> mReaderCount;
    size_t mWaitingWriterCount = 0;
};
}  // namespace Bedrock::Threading
