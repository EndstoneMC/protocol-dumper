#include "models.h"

namespace proto {

bool Constraints::empty() const
{
    return !mMinimum && !mMaximum && !mMinLength && !mMaxLength && !mMinItems && !mMaxItems && !mPattern;
}

}  // namespace proto
