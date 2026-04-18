#include "visitor.h"

#include <algorithm>
#include <print>
#include <ranges>
#include <unordered_map>
#include <utility>

#include "cereal/schema/BasicSchema.h"
#include "util.h"

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
        return big_end ? "float_be" : "float";
    case ReflectedType::Double:
        return big_end ? "double_be" : "double";

    case ReflectedType::Int8:
        return compress ? "varint32" : "int8";
    case ReflectedType::Uint8:
        return compress ? "uvarint32" : "uint8";

    case ReflectedType::Int16:
        if (compress) {
            return "varint32";
        }
        return big_end ? "int16_be" : "int16";
    case ReflectedType::Uint16:
        if (compress) {
            return "uvarint32";
        }
        return big_end ? "uint16_be" : "uint16";

    case ReflectedType::Int32:
        if (compress) {
            return "varint32";
        }
        return big_end ? "int32_be" : "int32";
    case ReflectedType::Uint32:
        if (compress) {
            return "uvarint32";
        }
        return big_end ? "uint32_be" : "uint32";

    case ReflectedType::Int64:
        if (compress) {
            return "varint64";
        }
        return big_end ? "int64_be" : "int64";
    case ReflectedType::Uint64:
        if (compress) {
            return "uvarint64";
        }
        return big_end ? "uint64_be" : "uint64";

    default:
        return "unknown";
    }
}

std::string lengthWire(SerializationTraits traits)
{
    return hasFlag(traits, SerializationTraits::NoSizeCompression) ? "uint32" : "uvarint32";
}

Constraints buildConstraints(const ConstraintDescription &c)
{
    Constraints out;
    out.mMinimum = c.mMinimum;
    out.mMaximum = c.mMaximum;
    out.mMinLength = c.mMinLength;
    out.mMaxLength = c.mMaxLength;
    out.mMinItems = c.mMinItems;
    out.mMaxItems = c.mMaxItems;
    out.mPattern = c.mPattern;
    return out;
}

// Maps primitive C++ types to wire type names via entt::type_info comparison.
// Returns empty string for user-defined types.
std::string_view primitiveWire(const entt::meta_type &type)
{
    const auto id = type.info();
    if (id == entt::type_id<bool>()) {
        return "bool";
    }
    if (id == entt::type_id<int8_t>()) {
        return "int8";
    }
    if (id == entt::type_id<uint8_t>()) {
        return "uint8";
    }
    if (id == entt::type_id<int16_t>()) {
        return "int16";
    }
    if (id == entt::type_id<uint16_t>()) {
        return "uint16";
    }
    if (id == entt::type_id<int32_t>()) {
        return "int32";
    }
    if (id == entt::type_id<uint32_t>()) {
        return "uint32";
    }
    if (id == entt::type_id<int64_t>()) {
        return "int64";
    }
    if (id == entt::type_id<uint64_t>()) {
        return "uint64";
    }
    if (id == entt::type_id<float>()) {
        return "float";
    }
    if (id == entt::type_id<double>()) {
        return "double";
    }
    if (id == entt::type_id<std::string>()) {
        return "string";
    }
    return {};
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

std::string qualifiedName(const entt::meta_type &t)
{
    return std::string{stripTypePrefix(t.info().name())};
}

cereal::internal::BasicSchema *getSchema(const entt::meta_type &t)
{
    auto *d = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(t.custom());
    return (d && d->mPtr) ? d->mPtr.get() : nullptr;
}

struct PacketSource {
    int id;
    std::string name;
    std::string description;
    cereal::SchemaDescription desc;
};

auto collectSchemas(const entt::meta_ctx &meta_ctx, const cereal::ReflectionCtx &ctx,
                    const cereal::DescriptionConfig &config)
    -> std::pair<std::vector<PacketSource>, std::unordered_map<std::string, cereal::SchemaDescription>>
{
    std::vector<PacketSource> packet_sources;
    std::unordered_map<std::string, cereal::SchemaDescription> type_sources;

    for (auto &&meta_type : entt::resolve(meta_ctx) | std::views::values) {
        auto *descriptor = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(meta_type.custom());
        if (!descriptor || !descriptor->mPtr) {
            continue;
        }

        auto name = qualifiedName(meta_type);
        descriptor->mName = name;

        if (descriptor->mUserPropertiesMap.contains("[cereal:packet]")) {
            int packet_id = entt::any_cast<int>(descriptor->mUserPropertiesMap["[cereal:packet]"].second);

            std::string details;
            if (auto it = descriptor->mUserPropertiesMap.find("[cereal:packet_details]");
                it != descriptor->mUserPropertiesMap.end()) {
                details = trim(entt::any_cast<const char *>(it->second.second));
            }

            cereal::internal::BasicSchema *payload_schema = nullptr;
            for (auto &&[func_id, func] : meta_type.func()) {
                if (func.arity() != 0) {
                    continue;
                }
                auto return_type = func.ret();
                payload_schema = getSchema(return_type);
                if (payload_schema) {
                    break;
                }
            }
            if (!payload_schema) {
                std::println(stderr, "ERROR - [{}] {}: failed to find payload schema", packet_id, name);
                continue;
            }
            packet_sources.push_back(
                {packet_id, name, std::move(details), payload_schema->description(ctx.internal(), config)});
        }
        else {
            type_sources.emplace(name, descriptor->mPtr->description(ctx.internal(), config));
        }
    }
    return {std::move(packet_sources), std::move(type_sources)};
}

Visitor::AliasMap buildAliasMap(const entt::meta_ctx &meta_ctx,
                                const std::unordered_map<std::string, cereal::SchemaDescription> &type_sources)
{
    Visitor::AliasMap aliases;
    for (auto &&meta_type : entt::resolve(meta_ctx) | std::views::values) {
        auto from = qualifiedName(meta_type);
        if (!type_sources.contains(from)) {
            continue;
        }
        for (auto &&[func_id, func] : meta_type.func()) {
            if (func.arity() != 0) {
                continue;
            }
            auto to = qualifiedName(func.ret());
            if (to == from) {
                continue;
            }
            if (auto it = type_sources.find(to); it != type_sources.end()) {
                aliases.emplace(from, &it->second);
                break;
            }
        }
    }
    // Collapse alias chains: a -> b -> c becomes a -> c.
    for (auto &[from, target] : aliases) {
        std::unordered_map<std::string, bool> seen;
        while (target) {
            auto target_name = target->mName.value_or("");
            if (!seen.emplace(target_name, true).second) {
                break;
            }
            auto next = aliases.find(target_name);
            if (next == aliases.end() || next->second == target) {
                break;
            }
            target = next->second;
        }
    }
    return aliases;
}

}  // namespace

Visitor::Visitor(const cereal::ReflectionCtx &ctx, const cereal::DescriptionConfig &config, AliasMap aliases)
    : mAliases(std::move(aliases)), mCtx(ctx), mConfig(config), mMetaCtx(ctx.internal().mMetaCtx)
{
}

Visitor::Resolved Visitor::buildVariant(const entt::meta_type &variantType)
{
    VariantType vt;
    if (!variantType) {
        return {std::move(vt), {}, {}};
    }

    if (cereal::internal::BasicSchema::TaggedVariantDescriptor *tvd = variantType.custom()) {
        if (tvd->mResolve) {
            if (auto tagType = tvd->mResolve(mMetaCtx)) {
                vt.switch_enum = stripAnonymousNamespace(std::string{stripTypePrefix(tagType.info().name())});
                // Resolve the tag enum's wire encoding from its schema.
                // Tag enums are always value-encoded (never serialized as strings).
                if (auto *tagSchema = getSchema(tagType)) {
                    auto tagDesc = tagSchema->description(mCtx.internal(), mConfig);
                    auto traits = getTraits(tagDesc);
                    tagDesc.mSerializationTraits = static_cast<SerializationTraits>(
                        static_cast<uint8_t>(traits) | static_cast<uint8_t>(SerializationTraits::EnumAsValue));
                    auto tagResolved = visitEnum(tagDesc);
                    if (auto *wire = std::get_if<std::string>(&tagResolved.spec)) {
                        vt.switch_type = std::move(*wire);
                    }
                }
            }
        }
        vt.switch_name = tvd->mTaggedName;
        if (!vt.switch_enum) {
            vt.switch_enum = tvd->mTaggedName;
        }
    }

    const auto arity = variantType.template_arity();
    vt.cases.reserve(arity);
    for (auto i = 0; i < arity; ++i) {
        auto alt = variantType.template_arg(i);
        if (!alt) {
            continue;
        }
        if (alt.info() == entt::type_id<cereal::NullType>()) {
            vt.cases.emplace_back();
        }
        else if (auto wire = primitiveWire(alt); !wire.empty()) {
            vt.cases.emplace_back(wire);
        }
        else {
            auto name = stripTypePrefix(alt.info().name());
            vt.cases.emplace_back(stripAnonymousNamespace(std::string{name}));
        }
    }
    return {std::move(vt), {}, {}};
}

Visitor::Resolved Visitor::visit(const cereal::SchemaDescription &desc)
{
    const auto rt = desc.mType.value_or(ReflectedType::Null);
    if (isScalarRT(rt)) {
        return visitScalar(desc);
    }
    switch (rt) {
    case ReflectedType::Enum:
        return visitEnum(desc);
    case ReflectedType::Object:
        return visitObject(desc);
    case ReflectedType::SequenceContainer:
        return visitArray(desc);
    case ReflectedType::AssociativeContainer:
        return visitMap(desc);
    default:
        return {};
    }
}

Visitor::Resolved Visitor::visitScalar(const cereal::SchemaDescription &desc)
{
    return {resolveWire(*desc.mType, getTraits(desc)), {}, {}};
}

Visitor::Resolved Visitor::visitEnum(const cereal::SchemaDescription &desc)
{
    const auto traits = getTraits(desc);
    const bool as_value = hasFlag(traits, SerializationTraits::EnumAsValue);
    std::string wire;
    if (as_value && desc.mUnderlyingType) {
        const auto remaining = static_cast<SerializationTraits>(
            static_cast<uint8_t>(traits) & ~static_cast<uint8_t>(SerializationTraits::EnumAsValue));
        wire = resolveWire(*desc.mUnderlyingType, remaining);
    }
    else {
        wire = "string";
    }
    return {std::move(wire), desc.mName.value_or(""), {}};
}

Visitor::Resolved Visitor::visitObject(const cereal::SchemaDescription &desc)
{
    const auto &name = desc.mName.value_or("");
    if (auto it = mAliases.find(std::string_view{name}); it != mAliases.end() && it->second) {
        return visit(*it->second);
    }
    return {stripAnonymousNamespace(name), {}, {}};
}

Visitor::Resolved Visitor::visitArray(const cereal::SchemaDescription &desc)
{
    auto inner = desc.mValueType ? visit(*desc.mValueType) : Resolved{};
    inner.repeat = lengthWire(getTraits(desc));
    return inner;
}

Visitor::Resolved Visitor::visitMap(const cereal::SchemaDescription &desc)
{
    auto key = desc.mKeyType ? visit(*desc.mKeyType) : Resolved{};
    auto val = desc.mMappedType ? visit(*desc.mMappedType) : Resolved{};
    MapType mt;
    mt.key = std::holds_alternative<std::string>(key.spec) ? std::get<std::string>(key.spec) : "unknown";
    mt.value = std::holds_alternative<std::string>(val.spec) ? std::get<std::string>(val.spec) : "unknown";
    return {std::move(mt), {}, lengthWire(getTraits(desc))};
}

Field Visitor::visitField(const std::string &name, const Member &member, const entt::meta_type &parent_meta)
{
    Field field;
    field.name = name;
    field.deprecated = member.mDeprecated;

    entt::meta_type declaredType;
    if (parent_meta) {
        const auto id = entt::hashed_string::value(name.c_str(), name.size());
        if (auto md = parent_meta.data(id)) {
            declaredType = md.type();
            if (isOptional(declaredType)) {
                field.optional = true;
                if (declaredType.template_arity() > 0) {
                    declaredType = declaredType.template_arg(0);
                }
            }
            if (cereal::internal::BasicSchema::MemberDescriptor *mdesc = md.custom()) {
                field.deprecated = mdesc->mIsDeprecatedComponent;
            }
        }
    }
    if (member.mDescription) {
        field.description = trim(*member.mDescription);
    }
    if (member.mConstraint) {
        auto c = buildConstraints(*member.mConstraint);
        if (!c.empty()) {
            field.constraints = std::move(c);
        }
    }

    if (isVariant(declaredType)) {
        auto resolved = buildVariant(declaredType);
        // mControlValueType describes the wire encoding of the variant discriminator.
        // It takes precedence over the type-level resolution from buildVariant.
        if (auto *vt = std::get_if<VariantType>(&resolved.spec); vt && member.mControlValueType) {
            vt->switch_type = lengthWire(getTraits(member));
        }
        field.type = std::move(resolved.spec);
        field.enum_name = std::move(resolved.enum_name);
        field.repeat = std::move(resolved.repeat);
        return field;
    }

    auto resolved = visit(member);

    // If the object type name is empty, fill it from the declared type.
    if (declaredType) {
        if (auto *s = std::get_if<std::string>(&resolved.spec); s && s->empty()) {
            *s = stripAnonymousNamespace(std::string{stripTypePrefix(declaredType.info().name())});
        }
    }

    field.type = std::move(resolved.spec);
    field.enum_name = std::move(resolved.enum_name);
    field.repeat = std::move(resolved.repeat);
    return field;
}

std::vector<Field> Visitor::visitStructFields(const cereal::SchemaDescription &desc)
{
    if (!desc.mMembers) {
        return {};
    }

    entt::meta_type parent = desc.mId ? entt::resolve(mMetaCtx, desc.mId) : entt::meta_type{};

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

    std::vector<Field> fields;
    fields.reserve(entries.size());
    for (const auto &e : entries) {
        fields.push_back(visitField(*e.name, *e.member, parent));
    }

    // Resolve tagged variant wire encodings from sibling enum fields (last resort).
    for (auto &field : fields) {
        auto *vt = std::get_if<VariantType>(&field.type);
        if (!vt || !vt->switch_enum || !vt->switch_type.empty()) {
            continue;
        }
        for (const auto &sibling : fields) {
            if (sibling.enum_name == *vt->switch_enum) {
                if (auto *wire = std::get_if<std::string>(&sibling.type)) {
                    vt->switch_type = *wire;
                }
                break;
            }
        }
    }

    return fields;
}

TypeDef Visitor::visitType(const std::string &name, const cereal::SchemaDescription &desc)
{
    TypeDef td;
    td.name = name;

    if (desc.mEnumValues && !desc.mEnumValues->empty()) {
        std::vector<EnumEntry> entries;
        entries.reserve(desc.mEnumValues->size());
        for (const auto &ev : *desc.mEnumValues) {
            EnumEntry e;
            e.name = ev.mName;
            e.value = ev.mValue;
            if (ev.mDescription && !ev.mDescription->empty()) {
                e.description = trim(*ev.mDescription);
            }
            entries.push_back(std::move(e));
        }
        td.body = std::move(entries);
        return td;
    }

    td.body = visitStructFields(desc);
    return td;
}

Packet Visitor::visitPacket(int id, const std::string &name, const cereal::SchemaDescription &desc)
{
    Packet pkt;
    pkt.id = id;
    pkt.name = name;
    pkt.fields = visitStructFields(desc);
    return pkt;
}

DumpResult dumpProtocol(const cereal::ReflectionCtx &ctx, const cereal::DescriptionConfig &config)
{
    auto &meta_ctx = ctx.internal().mMetaCtx;
    auto [packet_sources, type_sources] = collectSchemas(meta_ctx, ctx, config);
    auto aliases = buildAliasMap(meta_ctx, type_sources);

    Visitor visitor{ctx, config, aliases};
    DumpResult result;
    for (const auto &[name, desc] : type_sources) {
        if (aliases.contains(name)) {
            continue;
        }
        result.types.push_back(visitor.visitType(name, desc));
    }
    for (const auto &src : packet_sources) {
        auto pkt = visitor.visitPacket(src.id, src.name, src.desc);
        pkt.description = src.description;
        result.packets.push_back(std::move(pkt));
    }
    return result;
}

}  // namespace proto
