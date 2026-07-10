#pragma once

#include <memory>

namespace Bedrock {
class EnableNonOwnerReferences {
public:
    EnableNonOwnerReferences();
    EnableNonOwnerReferences(const EnableNonOwnerReferences &);
    virtual ~EnableNonOwnerReferences();
    void testOnly_initControlBlock();
    void testOnly_destroyControlBlock();
    EnableNonOwnerReferences &operator=(const EnableNonOwnerReferences &);

private:
    template <typename T>
    friend class NonOwnerPointer;
    struct ControlBlock {
        ControlBlock();
        bool mIsValid;
    };
    void _destroyControlBlock();
    std::shared_ptr<ControlBlock> mControlBlock;
};
}  // namespace Bedrock
