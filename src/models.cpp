#include "models.h"

namespace proto::model {

bool Constraints::empty() const
{
    return !mMinimum && !mMaximum && !mMinLength && !mMaxLength && !mMinItems && !mMaxItems && !mPattern;
}

}  // namespace proto::model
