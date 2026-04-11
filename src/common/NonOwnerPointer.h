#pragma once

#include "EnableNonOwnerReferences.h"

namespace Bedrock {
template <typename T>
class NonOwnerPointer {
public:
    NonOwnerPointer();
    NonOwnerPointer(std::nullptr_t);
    NonOwnerPointer(T *);
    NonOwnerPointer(T &);
    NonOwnerPointer(const NonOwnerPointer &rhs);
    NonOwnerPointer(NonOwnerPointer &&rhs);
    ~NonOwnerPointer();
    NonOwnerPointer &operator=(T *rhs);
    NonOwnerPointer &operator=(T &);
    NonOwnerPointer &operator=(const NonOwnerPointer &);
    NonOwnerPointer &operator=(NonOwnerPointer &&);
    NonOwnerPointer &operator=(std::nullptr_t);
    void reset();
    void resetDanglingPointer_testOnly();
    T *operator->();
    T &operator*();
    T *operator->() const;
    T &operator*() const;
    T *access() const;
    explicit operator bool() const;
    bool isValid() const;
    void swap(NonOwnerPointer &);
    bool operator==(const NonOwnerPointer &) const;
    bool operator!=(const NonOwnerPointer &) const;
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    long getOwnerUseCount() const;

private:
    void _setControlBlock(const EnableNonOwnerReferences *ptr);
    T *_get() const;
    NonOwnerPointer(std::shared_ptr<EnableNonOwnerReferences::ControlBlock>, T *);
    std::shared_ptr<EnableNonOwnerReferences::ControlBlock> mControlBlock;
    T *mPointer;
};
}  // namespace Bedrock
