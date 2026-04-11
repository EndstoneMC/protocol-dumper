#pragma once
// Bedrock::EnableNonOwnerReferences — ABI stub.
// BDS uses this as a weak-ref base class.  We only need the layout.

namespace Bedrock {

class EnableNonOwnerReferences {
protected:
    virtual ~EnableNonOwnerReferences() = default;
    void *_sharing_ref{nullptr};
};

}  // namespace Bedrock
