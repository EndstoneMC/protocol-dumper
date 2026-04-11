#pragma once

#include <memory>

#include "EnableNonOwnerReferences.h"

namespace Bedrock {
template <typename T>
class NonOwnerPointer {
public:
    NonOwnerPointer();
    NonOwnerPointer(std::nullptr_t);
    NonOwnerPointer(T *);
    NonOwnerPointer(T &);
    NonOwnerPointer(const NonOwnerPointer &rhs) = default;
    NonOwnerPointer(NonOwnerPointer &&rhs) = default;
    ~NonOwnerPointer() { reset(); }
    NonOwnerPointer &operator=(T *rhs);
    NonOwnerPointer &operator=(T &);
    NonOwnerPointer &operator=(const NonOwnerPointer &);
    NonOwnerPointer &operator=(NonOwnerPointer &&);
    NonOwnerPointer &operator=(std::nullptr_t);
    void reset()
    {
        mControlBlock.reset();
        mPointer = nullptr;
    }
    T *operator->() { return _get(); }
    T &operator*() { return *_get(); }
    T *operator->() const { return _get(); }
    T &operator*() const { return *_get(); }
    T *access() const { return mPointer; }
    explicit operator bool() const { return isValid(); }
    bool isValid() const { return mControlBlock && mControlBlock->mIsValid; }
    void swap(NonOwnerPointer &);
    bool operator==(const NonOwnerPointer &) const;
    bool operator!=(const NonOwnerPointer &) const;
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    long getOwnerUseCount() const;

private:
    void _setControlBlock(const EnableNonOwnerReferences *ptr);
    T *_get() const { return mPointer; }
    NonOwnerPointer(std::shared_ptr<EnableNonOwnerReferences::ControlBlock>, T *);
    std::shared_ptr<EnableNonOwnerReferences::ControlBlock> mControlBlock;
    T *mPointer;
};
}  // namespace Bedrock
