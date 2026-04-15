#include "visitor.h"

#include <algorithm>
#include <utility>

namespace proto {

using cereal::SerializationTraits;
using cereal::internal::ConstraintDescription;
using cereal::internal::Member;
using cereal::internal::ReflectedType;

namespace {

bool hasFlag(SerializationTraits traits, SerializationTraits flag)
{
    return (static_cast<uint8_t>(traits) & static_cast<uint8_t>(flag)) != 0;
}

SerializationTraits getTraits(const cereal::SchemaDescription &desc)
{
    return desc.mSerializationTraits.value_or(SerializationTraits::None);
}

std::string resolveWire(ReflectedType type, SerializationTraits traits)
{
    const bool compress = hasFlag(traits, SerializationTraits::Compression);
    const bool big_end = hasFlag(traits, SerializationTraits::BigEndian);

    switch (type) {
    case ReflectedType::Bool:   return "bool";
    case ReflectedType::String: return "string";
    case ReflectedType::Float:  return "float_le";
    case ReflectedType::Double: return "double_le";

    case ReflectedType::Int8:  return compress ? "zigzag32" : "int8";
    case ReflectedType::Uint8: return compress ? "varint32" : "uint8";

    case ReflectedType::Int16:
        if (compress) return "zigzag32";
        return big_end ? "int16_be" : "int16_le";
    case ReflectedType::Uint16:
        if (compress) return "varint32";
        return big_end ? "uint16_be" : "uint16_le";

    case ReflectedType::Int32:
        if (compress) return "zigzag32";
        return big_end ? "int32_be" : "int32_le";
    case ReflectedType::Uint32:
        if (compress) return "varint32";
        return big_end ? "uint32_be" : "uint32_le";

    case ReflectedType::Int64:
        if (compress) return "zigzag64";
        return big_end ? "int64_be" : "int64_le";
    case ReflectedType::Uint64:
        if (compress) return "varint64";
        return big_end ? "uint64_be" : "uint64_le";

    default: return "unknown";
    }
}

std::string lengthWire(SerializationTraits traits)
{
    return hasFlag(traits, SerializationTraits::NoSizeCompression) ? "uint32_le" : "varint32";
}

model::Constraints buildConstraints(const ConstraintDescription &c)
{
    model::Constraints out;
    out.mMinimum = c.mMinimum;
    out.mMaximum = c.mMaximum;
    out.mMinLength = c.mMinLength;
    out.mMaxLength = c.mMaxLength;
    out.mMinItems = c.mMinItems;
    out.mMaxItems = c.mMaxItems;
    out.mPattern = c.mPattern;
    return out;
}

bool isScalarRT(ReflectedType rt)
{
    switch (rt) {
    case ReflectedType::Bool:
    case ReflectedType::String:
    case ReflectedType::Int8:
    case ReflectedType::Uint8:
    case ReflectedType::Int16:
    case ReflectedType::Uint16:
    case ReflectedType::Int32:
    case ReflectedType::Uint32:
    case ReflectedType::Int64:
    case ReflectedType::Uint64:
    case ReflectedType::Float:
    case ReflectedType::Double:
        return true;
    default:
        return false;
    }
}

template <typename T>
class ScopedCursor {
public:
    ScopedCursor(T *&slot, T *value) : mSlot(slot), mSaved(slot) { slot = value; }
    ~ScopedCursor() { mSlot = mSaved; }
    ScopedCursor(const ScopedCursor &) = delete;
    ScopedCursor &operator=(const ScopedCursor &) = delete;

private:
    T *&mSlot;
    T *mSaved;
};

}  // namespace

void Visitor::setAliases(std::unordered_map<std::string, const cereal::SchemaDescription *> aliases)
{
    mAliases = std::move(aliases);
}

bool Visitor::isAlias(std::string_view name) const
{
    return mAliases.find(std::string(name)) != mAliases.end();
}

void Visitor::visit(const cereal::SchemaDescription &desc)
{
    const auto rt = desc.mType.value_or(ReflectedType::Null);
    if (isScalarRT(rt)) {
        visitScalar(desc);
        return;
    }
    switch (rt) {
    case ReflectedType::Enum:                 visitEnum(desc);   break;
    case ReflectedType::Object:               visitObject(desc); break;
    case ReflectedType::SequenceContainer:    visitArray(desc);  break;
    case ReflectedType::AssociativeContainer: visitMap(desc);    break;
    default:                                  break;
    }
}

void Visitor::visitScalar(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) return;
    auto ft = std::make_unique<model::ScalarFieldType>();
    ft->mWire = resolveWire(*desc.mType, getTraits(desc));
    *mTypeSlot = std::move(ft);
}

void Visitor::visitEnum(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) return;
    const auto traits = getTraits(desc);
    const bool as_value = hasFlag(traits, SerializationTraits::EnumAsValue);
    auto ft = std::make_unique<model::EnumFieldType>();
    ft->mTypeName = desc.mName.value_or("");
    if (as_value && desc.mUnderlyingType) {
        const auto remaining = static_cast<SerializationTraits>(
            static_cast<uint8_t>(traits) & ~static_cast<uint8_t>(SerializationTraits::EnumAsValue));
        ft->mWire = resolveWire(*desc.mUnderlyingType, remaining);
    }
    else {
        ft->mWire = "string";
    }
    *mTypeSlot = std::move(ft);
}

void Visitor::visitObject(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) return;
    const auto name = desc.mName.value_or("");
    if (auto it = mAliases.find(name); it != mAliases.end() && it->second) {
        visit(*it->second);
        return;
    }
    auto ft = std::make_unique<model::ObjectFieldType>();
    ft->mTypeName = name;
    *mTypeSlot = std::move(ft);
}

void Visitor::visitArray(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) return;
    auto ft = std::make_unique<model::ArrayFieldType>();
    ft->mLengthWire = lengthWire(getTraits(desc));
    auto *raw = ft.get();
    *mTypeSlot = std::move(ft);
    if (desc.mValueType) {
        ScopedCursor<std::unique_ptr<model::FieldType>> guard{mTypeSlot, &raw->mElement};
        visit(*desc.mValueType);
    }
}

void Visitor::visitMap(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) return;
    auto ft = std::make_unique<model::MapFieldType>();
    ft->mLengthWire = lengthWire(getTraits(desc));
    auto *raw = ft.get();
    *mTypeSlot = std::move(ft);
    if (desc.mKeyType) {
        ScopedCursor<std::unique_ptr<model::FieldType>> guard{mTypeSlot, &raw->mKey};
        visit(*desc.mKeyType);
    }
    if (desc.mMappedType) {
        ScopedCursor<std::unique_ptr<model::FieldType>> guard{mTypeSlot, &raw->mValue};
        visit(*desc.mMappedType);
    }
}

void Visitor::visitField(const std::string &name, const Member &member)
{
    model::Field *target = nullptr;
    if (mCurrentClass) {
        auto field = std::make_unique<model::Field>();
        target = field.get();
        mCurrentClass->mMembers.push_back(std::move(field));
    }
    else if (mCurrentPacket) {
        mCurrentPacket->mFields.emplace_back();
        target = &mCurrentPacket->mFields.back();
    }
    else {
        return;
    }

    target->mName = name;
    target->mRequired = member.mRequired;
    target->mDeprecated = member.mDeprecated;
    if (member.mDescription) {
        target->mDescription = *member.mDescription;
    }
    if (member.mConstraint) {
        auto c = buildConstraints(*member.mConstraint);
        if (!c.empty()) {
            target->mConstraints = std::move(c);
        }
    }

    ScopedCursor<std::unique_ptr<model::FieldType>> guard{mTypeSlot, &target->mType};
    visit(member);
}

void Visitor::visitStructFields(const cereal::SchemaDescription &desc)
{
    if (!desc.mMembers) return;

    struct Entry {
        const std::string *name;
        const Member *member;
        unsigned char ordinal;
    };
    std::vector<Entry> entries;
    entries.reserve(desc.mMembers->size());
    for (const auto &[n, m] : *desc.mMembers) {
        entries.push_back({&n, &m, m.mOrdinalIndex.value_or(255)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.ordinal < b.ordinal; });

    for (const auto &e : entries) {
        visitField(*e.name, *e.member);
    }
}

void Visitor::visitClass(const std::string &name, const cereal::SchemaDescription &desc)
{
    if (isAlias(name)) return;

    auto cls = std::make_unique<model::Class>();
    cls->mName = name;

    if (desc.mEnumValues && !desc.mEnumValues->empty()) {
        cls->mKind = model::ClassKind::Enum;
        for (const auto &ev : *desc.mEnumValues) {
            auto v = std::make_unique<model::EnumValue>();
            v->mName = ev.mName;
            v->mValue = ev.mValue;
            if (ev.mDescription && !ev.mDescription->empty()) {
                v->mDescription = *ev.mDescription;
            }
            cls->mMembers.push_back(std::move(v));
        }
        mClasses.push_back(std::move(cls));
        return;
    }

    cls->mKind = model::ClassKind::Struct;
    auto *raw = cls.get();
    mClasses.push_back(std::move(cls));

    ScopedCursor<model::Class> guard{mCurrentClass, raw};
    visitStructFields(desc);
}

void Visitor::visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc)
{
    model::Packet pkt;
    pkt.mId = id;
    pkt.mName = name;
    mPackets.push_back(std::move(pkt));

    ScopedCursor<model::Packet> guard{mCurrentPacket, &mPackets.back()};
    visitStructFields(desc);
}

}  // namespace proto
