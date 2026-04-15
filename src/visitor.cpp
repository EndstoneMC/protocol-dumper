#include "visitor.h"

#include <algorithm>
#include <utility>

#include "cereal/schema/BasicSchema.h"

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
    case ReflectedType::Bool:
        return "bool";
    case ReflectedType::String:
        return "string";
    case ReflectedType::Float:
        return "float_le";
    case ReflectedType::Double:
        return "double_le";

    case ReflectedType::Int8:
        return compress ? "zigzag32" : "int8";
    case ReflectedType::Uint8:
        return compress ? "varint32" : "uint8";

    case ReflectedType::Int16:
        if (compress) {
            return "zigzag32";
        }
        return big_end ? "int16_be" : "int16_le";
    case ReflectedType::Uint16:
        if (compress) {
            return "varint32";
        }
        return big_end ? "uint16_be" : "uint16_le";

    case ReflectedType::Int32:
        if (compress) {
            return "zigzag32";
        }
        return big_end ? "int32_be" : "int32_le";
    case ReflectedType::Uint32:
        if (compress) {
            return "varint32";
        }
        return big_end ? "uint32_be" : "uint32_le";

    case ReflectedType::Int64:
        if (compress) {
            return "zigzag64";
        }
        return big_end ? "int64_be" : "int64_le";
    case ReflectedType::Uint64:
        if (compress) {
            return "varint64";
        }
        return big_end ? "uint64_be" : "uint64_le";

    default:
        return "unknown";
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

std::string_view stripTypePrefix(std::string_view name)
{
    for (auto p : {"struct ", "class ", "enum ", "union "}) {
        if (name.starts_with(p)) {
            name.remove_prefix(std::string_view{p}.size());
            break;
        }
    }
    return name;
}

bool isOptional(const entt::meta_type &t)
{
    return t && stripTypePrefix(t.info().name()).starts_with("std::optional<");
}

bool isVariant(const entt::meta_type &t)
{
    return t && stripTypePrefix(t.info().name()).starts_with("std::variant<");
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
class ScopedAssign {
public:
    ScopedAssign(T &slot, T value) : mSlot(slot), mSaved(std::exchange(slot, std::move(value))) {}
    ~ScopedAssign() { mSlot = std::move(mSaved); }
    ScopedAssign(const ScopedAssign &) = delete;
    ScopedAssign &operator=(const ScopedAssign &) = delete;

private:
    T &mSlot;
    T mSaved;
};

}  // namespace

Visitor::Visitor(const entt::meta_ctx &ctx, AliasMap aliases) : mAliases(std::move(aliases)), mMetaCtx(ctx) {}

std::unique_ptr<model::FieldType> Visitor::buildVariant(const entt::meta_type &variantType) const
{
    auto vt = std::make_unique<model::VariantFieldType>();
    if (!variantType) {
        return vt;
    }

    // TaggedVariantDescriptor is attached as custom on the variant *type* node itself.
    // mTaggedName is the tag field name on the parent; mResolve yields the tag's enum type.
    // The enum-value → alternative mapping is a follow-up once the enum binding is decompiled.
    if (cereal::internal::BasicSchema::TaggedVariantDescriptor *tvd = variantType.custom()) {
        vt->mTag.mName = tvd->mTaggedName;
        if (tvd->mResolve) {
            if (auto tagType = tvd->mResolve(mMetaCtx)) {
                vt->mTag.mTypeName = std::string{stripTypePrefix(tagType.info().name())};
            }
        }
    }

    const auto arity = variantType.template_arity();
    vt->mOf.reserve(arity);
    for (auto i = 0; i < arity; ++i) {
        auto alt = variantType.template_arg(i);
        if (!alt) {
            continue;
        }
        vt->mOf.emplace_back(stripTypePrefix(alt.info().name()));
    }
    return vt;
}

bool Visitor::isAlias(std::string_view name) const
{
    return mAliases.find(name) != mAliases.end();
}

void Visitor::visit(const cereal::SchemaDescription &desc)
{
    const auto rt = desc.mType.value_or(ReflectedType::Null);
    if (isScalarRT(rt)) {
        visitScalar(desc);
        return;
    }
    switch (rt) {
    case ReflectedType::Enum:
        visitEnum(desc);
        break;
    case ReflectedType::Object:
        visitObject(desc);
        break;
    case ReflectedType::SequenceContainer:
        visitArray(desc);
        break;
    case ReflectedType::AssociativeContainer:
        visitMap(desc);
        break;
    default:
        break;
    }
}

void Visitor::visitScalar(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) {
        return;
    }
    auto ft = std::make_unique<model::ScalarFieldType>();
    ft->mWire = resolveWire(*desc.mType, getTraits(desc));
    *mTypeSlot = std::move(ft);
}

void Visitor::visitEnum(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) {
        return;
    }
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
    if (!mTypeSlot) {
        return;
    }
    const auto &name = desc.mName.value_or("");
    if (auto it = mAliases.find(std::string_view{name}); it != mAliases.end() && it->second) {
        visit(*it->second);
        return;
    }
    auto ft = std::make_unique<model::ObjectFieldType>();
    ft->mTypeName = name;
    *mTypeSlot = std::move(ft);
}

void Visitor::visitArray(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) {
        return;
    }
    auto ft = std::make_unique<model::ArrayFieldType>();
    ft->mLengthWire = lengthWire(getTraits(desc));
    auto *raw = ft.get();
    *mTypeSlot = std::move(ft);
    if (desc.mValueType) {
        ScopedAssign slotGuard{mTypeSlot, &raw->mElement};
        visit(*desc.mValueType);
    }
}

void Visitor::visitMap(const cereal::SchemaDescription &desc)
{
    if (!mTypeSlot) {
        return;
    }
    auto ft = std::make_unique<model::MapFieldType>();
    ft->mLengthWire = lengthWire(getTraits(desc));
    auto *raw = ft.get();
    *mTypeSlot = std::move(ft);
    if (desc.mKeyType) {
        ScopedAssign slotGuard{mTypeSlot, &raw->mKey};
        visit(*desc.mKeyType);
    }
    if (desc.mMappedType) {
        ScopedAssign slotGuard{mTypeSlot, &raw->mValue};
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
    target->mRequired = true;
    target->mDeprecated = member.mDeprecated;
    entt::meta_type declaredType;
    if (mCurrentMetaType) {
        const auto id = entt::hashed_string::value(name.c_str(), name.size());
        if (auto md = mCurrentMetaType.data(id)) {
            declaredType = md.type();
            if (isOptional(declaredType)) {
                target->mRequired = false;
                if (declaredType.template_arity() > 0) {
                    declaredType = declaredType.template_arg(0);
                }
            }
            if (cereal::internal::BasicSchema::MemberDescriptor *mdesc = md.custom()) {
                target->mDeprecated = mdesc->mIsDeprecatedComponent;
            }
        }
    }
    if (member.mDescription) {
        target->mDescription = *member.mDescription;
    }
    if (member.mConstraint) {
        auto c = buildConstraints(*member.mConstraint);
        if (!c.empty()) {
            target->mConstraints = std::move(c);
        }
    }

    if (isVariant(declaredType)) {
        target->mType = buildVariant(declaredType);
        return;
    }

    {
        ScopedAssign slotGuard{mTypeSlot, &target->mType};
        visit(member);
    }

    if (declaredType) {
        if (auto *obj = dynamic_cast<model::ObjectFieldType *>(target->mType.get()); obj && obj->mTypeName.empty()) {
            obj->mTypeName = std::string{stripTypePrefix(declaredType.info().name())};
        }
    }
}

void Visitor::visitStructFields(const cereal::SchemaDescription &desc)
{
    if (!desc.mMembers) {
        return;
    }

    entt::meta_type parent = desc.mId ? entt::resolve(mMetaCtx, desc.mId) : entt::meta_type{};
    ScopedAssign metaGuard{mCurrentMetaType, parent};

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
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.ordinal < b.ordinal; });

    for (const auto &e : entries) {
        visitField(*e.name, *e.member);
    }
}

void Visitor::visitClass(const std::string &name, const cereal::SchemaDescription &desc)
{
    if (isAlias(name)) {
        return;
    }

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

    ScopedAssign classGuard{mCurrentClass, raw};
    visitStructFields(desc);
}

void Visitor::visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc)
{
    model::Packet pkt;
    pkt.mId = id;
    pkt.mName = name;
    mPackets.push_back(std::move(pkt));

    ScopedAssign packetGuard{mCurrentPacket, &mPackets.back()};
    visitStructFields(desc);
}

}  // namespace proto
