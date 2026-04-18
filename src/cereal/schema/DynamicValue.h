#pragma once
// Mirrors: src-deps/Cereal/include/Cereal/schema/dynamic/DynamicValue.h

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cereal {

struct NullType {};

class DynamicValue {
public:
    using Variant = std::variant<NullType, bool, int64_t, double, std::string, std::vector<DynamicValue>,
                                 std::unordered_map<std::string, DynamicValue>>;

    DynamicValue() = default;

    const Variant &variant() const { return mValue; }

private:
    Variant mValue;
};

}  // namespace cereal
