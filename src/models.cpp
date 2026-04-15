#include "models.h"

namespace proto::model {

bool Constraints::empty() const
{
    return !mMinimum && !mMaximum && !mMinLength && !mMaxLength && !mMinItems && !mMaxItems && !mPattern;
}

Json Constraints::toJson() const
{
    Json j = Json::object();
    if (mMinimum) j["minimum"] = *mMinimum;
    if (mMaximum) j["maximum"] = *mMaximum;
    if (mMinLength) j["min_length"] = *mMinLength;
    if (mMaxLength) j["max_length"] = *mMaxLength;
    if (mMinItems) j["min_items"] = *mMinItems;
    if (mMaxItems) j["max_items"] = *mMaxItems;
    if (mPattern) j["pattern"] = *mPattern;
    return j;
}

Json ScalarFieldType::toJson() const
{
    return Json{{"type", mWire}};
}

Json EnumFieldType::toJson() const
{
    return Json{{"type", mTypeName}, {"as", mWire}};
}

Json ObjectFieldType::toJson() const
{
    return Json{{"type", mTypeName}};
}

Json ArrayFieldType::toJson() const
{
    Json j = Json::object();
    j["type"] = "array";
    j["prefix"] = mLengthWire;
    if (mElement) {
        j["element"] = mElement->toJson();
    }
    return j;
}

Json MapFieldType::toJson() const
{
    Json j = Json::object();
    j["type"] = "map";
    j["prefix"] = mLengthWire;
    if (mKey) {
        j["key"] = mKey->toJson();
    }
    if (mValue) {
        j["value"] = mValue->toJson();
    }
    return j;
}

Json VariantFieldType::toJson() const
{
    Json j = Json::object();
    j["type"] = "variant";
    if (!mTag.mName.empty() || !mTag.mTypeName.empty()) {
        Json tag = Json::object();
        if (!mTag.mName.empty()) tag["name"] = mTag.mName;
        if (!mTag.mTypeName.empty()) tag["type"] = mTag.mTypeName;
        j["tag"] = std::move(tag);
    }
    j["of"] = mOf;
    return j;
}

Json Field::toJson() const
{
    Json j = Json::object();
    j["name"] = mName;
    if (mType) {
        j.update(mType->toJson());
    }
    if (!mRequired) {
        j["optional"] = true;
    }
    if (mDeprecated) {
        j["deprecated"] = true;
    }
    if (mDescription) {
        j["description"] = *mDescription;
    }
    if (mConstraints && !mConstraints->empty()) {
        j["constraints"] = mConstraints->toJson();
    }
    return j;
}

Json EnumValue::toJson() const
{
    Json j = Json::object();
    j["name"] = mName;
    j["value"] = mValue;
    if (mDescription) {
        j["description"] = *mDescription;
    }
    return j;
}

Json Class::toJson() const
{
    Json j = Json::object();
    j["name"] = mName;
    if (mKind == ClassKind::Enum) {
        j["kind"] = "enum";
        Json values = Json::array();
        for (const auto &m : mMembers) {
            if (m) values.push_back(m->toJson());
        }
        j["values"] = std::move(values);
    }
    else {
        Json fields = Json::array();
        for (const auto &m : mMembers) {
            if (m) fields.push_back(m->toJson());
        }
        j["fields"] = std::move(fields);
    }
    return j;
}

Json Packet::toJson() const
{
    Json j = Json::object();
    j["id"] = mId;
    j["name"] = mName;
    Json fields = Json::array();
    for (const auto &f : mFields) {
        fields.push_back(f.toJson());
    }
    j["fields"] = std::move(fields);
    return j;
}

}  // namespace proto::model
