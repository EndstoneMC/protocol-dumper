#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/dynamic/DynamicValue.h

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cereal {

namespace internal {
struct NullType {
    bool operator==(NullType) const { return true; }
};
}  // namespace internal

class DynamicValue {
public:
    using Variant = std::variant<
        internal::NullType, bool, int64_t, double, std::string,
        std::vector<DynamicValue>,
        std::unordered_map<std::string, DynamicValue>>;

    DynamicValue() = default;

    const Variant &variant() const { return mValue; }

private:
    Variant mValue;
};

}  // namespace cereal
