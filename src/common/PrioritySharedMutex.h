#pragma once

#include <condition_variable>
#include <mutex>
#include <shared_mutex>

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
    void lock() { mStrategy.lock(); }
    bool try_lock() { return mStrategy.try_lock(); }
    void unlock() { mStrategy.unlock(); }
    void lock_shared() { mStrategy.lock_shared(); }
    bool try_lock_shared() { return mStrategy.try_lock_shared(); }
    void unlock_shared() { mStrategy.unlock_shared(); }
    LockingStrategy &getStrategy() { return mStrategy; }

private:
    LockingStrategy mStrategy;
};

class PrioritizeSharedOwnership {
public:
    void lock_shared()
    {
        mMutex.lock();
        mReaderCount += 1;
        mMutex.unlock();
    }

    bool try_lock_shared()
    {
        if (!mMutex.try_lock_shared()) {
            return false;
        }
        mReaderCount += 1;
        mMutex.unlock_shared();
        return true;
    }

    void unlock_shared()
    {
        auto prev = mReaderCount.fetch_sub(1);
        if (prev == (mWaitForZeroBit | 1)) {
            mMutex.lock();
            mReaderCount.fetch_and(~mWaitForZeroBit);
            mMutex.unlock();
            mZeroReaders.notify_all();
        }
    }

    void lock()
    {
        mMutex.lock();
        auto count = mReaderCount.load(std::memory_order_acquire);
        while (count != 0) {
            auto desired = count | mWaitForZeroBit;
            if (mReaderCount.compare_exchange_weak(count, desired, std::memory_order_acq_rel)) {
                mZeroReaders.wait(*this);
                count = mReaderCount.load(std::memory_order_acquire);
            }
        }
    }

    bool try_lock()
    {
        if (!mMutex.try_lock()) {
            return false;
        }
        if (mReaderCount != 0) {
            mMutex.unlock();
            return false;
        }
        return true;
    }

    void unlock() { mMutex.unlock(); }

    size_t waitingWriters() const { return mWaitingWriterCount & ~mWaitForZeroBit; }

private:
    static constexpr size_t mWaitForZeroBit = 0x8000000000000000;
    SharedMutex mMutex;
    ConditionVariableAny mZeroReaders;
    std::atomic<size_t> mReaderCount{0};
    size_t mWaitingWriterCount = 0;
};
}  // namespace Bedrock::Threading
