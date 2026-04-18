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
    return Json(mWire);
}

Json EnumFieldType::toJson() const
{
    return Json(mWire);
}

Json ObjectFieldType::toJson() const
{
    return Json(mTypeName);
}

Json ArrayFieldType::toJson() const
{
    return mElement ? mElement->toJson() : Json();
}

Json MapFieldType::toJson() const
{
    Json j = Json::object();
    if (mKey) j["key"] = mKey->toJson();
    if (mValue) j["value"] = mValue->toJson();
    return j;
}

Json VariantFieldType::toJson() const
{
    Json j = Json::object();
    if (!mTag.mName.empty()) {
        j["switch-on"] = mTag.mName;
    }
    j["cases"] = mOf;
    return j;
}

Json Field::toJson() const
{
    Json j = Json::object();
    j["name"] = mName;
    if (mType) {
        j["type"] = mType->toJson();
        if (auto e = mType->enumName(); !e.empty()) {
            j["enum"] = std::move(e);
        }
        if (auto r = mType->repeat(); !r.empty()) {
            j["repeat"] = std::move(r);
        }
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
        j["kind"] = "struct";
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
