#include "visitor.h"

#include <limits>
#include <print>
#include <ranges>

#include "cereal/schema/BasicSchema.h"

namespace proto {

namespace {

std::string sanitise_typename(const entt::meta_type &type)
{
    if (type.info().hash() == entt::type_hash<std::string>()) {
        return "string";
    }

    constexpr std::string_view kPrefixes[] = {"struct ", "class ", "enum ", "union "};
    std::string_view name = type.info().name();
    for (auto p : kPrefixes) {
        if (name.starts_with(p)) {
            name.remove_prefix(p.size());
            break;
        }
    }

    std::string result(name);
    constexpr std::string_view kAnon = "(anonymous namespace)::";
    for (std::string::size_type pos; (pos = result.find(kAnon)) != std::string::npos;) {
        result.erase(pos, kAnon.size());
    }
    return result;
}

std::string trim(std::string_view s)
{
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return {};
    }
    return std::string(s.substr(start, s.find_last_not_of(" \t\n\r") - start + 1));
}

std::string primitive_name(cereal::internal::ReflectedType rt, cereal::SerializationTraits traits)
{
    using RT = cereal::internal::ReflectedType;
    const bool compressed = !!(traits & cereal::SerializationTraits::Compression);
    const bool big_endian = !!(traits & cereal::SerializationTraits::BigEndian);

    if (compressed) {
        switch (rt) {
        case RT::Int32:
            return "varint32";
        case RT::Uint32:
            return "uvarint32";
        case RT::Int64:
            return "varint64";
        case RT::Uint64:
            return "uvarint64";
        default:
            break;
        }
    }

    std::string name;
    bool sized_numeric = true;
    switch (rt) {
    case RT::Bool:
        name = "bool";
        sized_numeric = false;
        break;
    case RT::String:
        name = "string";
        sized_numeric = false;
        break;
    case RT::Int8:
        name = "int8";
        break;
    case RT::Uint8:
        name = "uint8";
        break;
    case RT::Int16:
        name = "int16";
        break;
    case RT::Uint16:
        name = "uint16";
        break;
    case RT::Int32:
        name = "int32";
        break;
    case RT::Uint32:
        name = "uint32";
        break;
    case RT::Int64:
        name = "int64";
        break;
    case RT::Uint64:
        name = "uint64";
        break;
    case RT::Float:
        name = "float";
        break;
    case RT::Double:
        name = "double";
        break;
    default:
        throw std::runtime_error(std::format("unsupported primitive reflected type: {}", static_cast<int>(rt)));
    }
    if (sized_numeric && big_endian) {
        name += "_be";
    }
    return name;
}

}  // namespace

Visitor::Visitor(const cereal::ReflectionCtx &ctx) : reflection_ctx_(ctx), meta_ctx_(ctx.internal().mMetaCtx)
{
    using Extra = cereal::DescriptionConfig::Extra;
    config_.mContextArea = cereal::ContextArea::ALL;
    config_.mExtraInfo = Extra::networkingExtraInfo | Extra::nonPublicFlag;
    config_.mIsTopLevel = true;
}

const Visitor::TypeMap &Visitor::getTypes() const
{
    if (types_.empty()) {
        auto &self = const_cast<Visitor &>(*this);
        for (const auto &type : entt::resolve(meta_ctx_) | std::views::values) {
            self.visit(type);
        }
    }
    return types_;
}

void Visitor::visit(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    const cereal::internal::BasicSchema::TypeDescriptor *descriptor = type.custom();
    if (!descriptor || !descriptor->mPtr) {
        return;
    }

    if (descriptor->mUserPropertiesMap.contains("[cereal:packet]")) {
        visitPacket(type);
        return;
    }

    entt::meta_type alias;
    for (const auto &[id, func] : type.func()) {
        if (cereal::internal::BasicSchema::SetterDescriptor *setter = func.custom()) {
            if (func.arity() <= 0) {
                throw std::runtime_error("setter must have arguments");
            }
            alias = func.arg(0);
        }
    }
    if (alias) {
        visitTypeAlias(type, alias);
        return;
    }

    if (type.is_enum()) {
        visitEnum(type);
        return;
    }

    visitType(type);
}

void Visitor::visitPacket(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    // std::println("Packet: {}", type.info().name());
    const cereal::internal::BasicSchema::TypeDescriptor *descriptor = type.custom();
    if (!descriptor) {
        throw std::runtime_error("packet missing type descriptor");
    }
    auto it = descriptor->mUserPropertiesMap.find("[cereal:packet]");
    Packet pk;
    pk.id = entt::any_cast<int>(it->second.second);
    pk.name = sanitise_typename(type);
    it = descriptor->mUserPropertiesMap.find("[cereal:packet_details]");
    if (it == descriptor->mUserPropertiesMap.end()) {
        throw std::runtime_error("packet without details");
    }
    std::string_view description = entt::any_cast<const char *>(it->second.second);
    if (!description.empty()) {
        pk.description = trim(description);
    }

    auto funcs = type.func();
    if (std::ranges::size(funcs) != 2) {
        throw std::runtime_error("packet has invalid getter and setter");
    }
    entt::meta_type read_as, write_as;
    for (const auto &[id, func] : type.func()) {
        if (cereal::internal::BasicSchema::GetterDescriptor *getter = func.custom()) {
            if (func.arity() > 0) {
                throw std::runtime_error("getter must not have argument");
            }
            read_as = func.ret();
        }
        else if (cereal::internal::BasicSchema::SetterDescriptor *setter = func.custom()) {
            if (func.arity() < 1) {
                throw std::runtime_error("setter must have at least one argument");
            }
            write_as = func.arg(0);
        }
    }
    if (!read_as || read_as != write_as) {
        throw std::runtime_error("invalid packet payload");
    }
    visit(read_as);
    if (auto payload_it = types_.find(read_as.id()); payload_it != types_.end()) {
        if (auto *payload = std::get_if<Type>(&payload_it->second)) {
            pk.payload = *payload;
        }
    }
    types_[type.id()] = std::move(pk);
}

void Visitor::visitTypeAlias(const entt::meta_type &type, const entt::meta_type &value)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    // std::println("TypeAlias: using {} = {}", type.info().name(), value.info().name());
    visit(value);
    TypeAlias type_alias;
    type_alias.name = sanitise_typename(type);
    if (types_.contains(value.id())) {
        auto &ty = types_.at(value.id());
        if (std::holds_alternative<TypeAlias>(ty)) {
            type_alias.value = std::get<TypeAlias>(ty).value;
        }
        else if (std::holds_alternative<Type>(ty)) {
            type_alias.value = std::get<Type>(ty);
        }
        else {
            throw std::runtime_error("invalid type alias");
        }
    }
    else {
        type_alias.value = sanitise_typename(value);
    }
    types_[type.id()] = std::move(type_alias);
}

void Visitor::visitEnum(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    // std::println("Enum: {}", type.info().name());
    Enum en;
    en.name = sanitise_typename(type);
    for (const auto &[id, data] : type.data()) {
        cereal::internal::BasicSchema::MemberDescriptor *descriptor = data.custom();
        if (!descriptor) {
            throw std::runtime_error("enum member missing type descriptor");
        }
        auto name = descriptor->mOriginalEnumName;
        auto value = data.get({});
        if (!value) {
            throw std::runtime_error(std::format("Invalid enum value {} in type {}", name, type.info().name()));
        }
        if (value.allow_cast<std::int64_t>()) {
            en.values.emplace_back(descriptor->mOriginalEnumName, value.cast<std::int64_t>());
        }
        else if (value.allow_cast<std::uint64_t>()) {
            en.values.emplace_back(descriptor->mOriginalEnumName, value.cast<std::uint64_t>());
        }
        else {
            throw std::runtime_error(
                std::format("Failed to cast enum value {} in type {} to a integer", name, type.info().name()));
        }
    }
    types_[type.id()] = std::move(en);
}

template <>
Field Visitor::visitMember<Field>(std::string_view name, const cereal::internal::Member &member)
{
    Field f;
    f.name = name;
    f.deprecated = member.mDeprecated;
    if (member.mDescription) {
        f.description = trim(*member.mDescription);
    }

    entt::meta_type type = entt::resolve(meta_ctx_, member.mId);
    if (type) {
        visit(type);
        std::visit(
            [&f](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Packet>) {
                    throw std::runtime_error("field can not have packet as type");
                }
                else {
                    f.type = arg;
                }
            },
            types_.at(type.id()));
    }
    else if (member.mType) {
        f.type = primitive_name(*member.mType, member.mSerializationTraits.value_or(cereal::SerializationTraits::None));
    }
    else {
        std::println("!Warning: {} has neither id nor reflected type, fall back to {}", name,
                     member.mName.value_or(std::string(name)));
    }

    return f;
}

template <>
MapField Visitor::visitMember<MapField>(std::string_view name, const cereal::internal::Member &member)
{
    MapField f;
    f.name = name;
    return f;
}

template <>
ArrayField Visitor::visitMember<ArrayField>(std::string_view name, const cereal::internal::Member &member)
{
    ArrayField f;
    f.name = name;
    return f;
}

void Visitor::visitType(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    // std::println("Type: {}", type.info().name());
    Type ty;
    ty.name = sanitise_typename(type);
    // if (desc.mMembers) {
    //     if (std::ranges::size(desc.mMembers.value()) != std::ranges::size(type.data())) {
    //         throw std::runtime_error(std::format("{} has {} members, expected {}", type.info().name(),
    //                                              std::ranges::size(desc.mMembers.value()),
    //                                              std::ranges::size(type.data())));
    //     }
    //
    //     std::println("Type: {}", type.info().name());
    //     for (const auto &data : type.data()) {
    //         std::println("Data: {} - {}", data.second.name(), data.second.type().info().name());
    //     }
    //     auto &members = desc.mMembers.value();
    //     std::map<int, FieldType> ordered;
    //     for (auto &[name, member] : members) {
    //         if (!member.mOrdinalIndex) {
    //             throw std::runtime_error(std::format("{} has no ordinal index", name));
    //         }
    //         if (ordered.contains(*member.mOrdinalIndex)) {
    //             throw std::runtime_error(std::format("{} has duplicate ordinal index", name));
    //         }
    //         auto index = member.mOrdinalIndex.value();
    //         auto rt = member.mType.value_or(cereal::internal::ReflectedType::Null);
    //         if (rt == cereal::internal::ReflectedType::AssociativeContainer) {
    //             ordered[index] = visitMember<MapField>(name, member);
    //         }
    //         else if (rt == cereal::internal::ReflectedType::SequenceContainer) {
    //             ordered[index] = visitMember<MapField>(name, member);
    //         }
    //         else {
    //             ordered[index] = visitMember<Field>(name, member);
    //         }
    //     }
    //     ty.fields.reserve(ordered.size());
    //     for (const auto &field : ordered | std::views::values) {
    //         ty.fields.emplace_back(std::move(field));
    //     }
    // }
    types_[type.id()] = std::move(ty);
}

bool Visitor::isVisited(const entt::meta_type &type) const
{
    return types_.contains(type.id());
}

}  // namespace proto
