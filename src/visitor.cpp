#include "visitor.h"

#include <limits>
#include <print>
#include <ranges>

#include "cereal/schema/BasicSchema.h"

namespace proto {

namespace {

std::string sanitise_typename(const entt::meta_type &type)
{
    switch (type.info().hash()) {
    case entt::type_hash<std::string>():
        return "string";
    default:
        break;
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

std::string serialization_type(const entt::meta_type &type, cereal::SerializationTraits traits)
{
    const auto size = type.size_of();
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        throw std::runtime_error(std::format("unsupported enum underlying size {} in {}", size, type.info().name()));
    }

    // Round-trip probe: int64(-1) -> enum -> int64. Unsigned underlying wraps the sign away.
    bool is_signed = type.is_signed();

    const bool compression = !!(traits & cereal::SerializationTraits::Compression);
    const bool big_endian = !!(traits & cereal::SerializationTraits::BigEndian);
    const std::string_view sign = is_signed ? "" : "u";

    if (compression) {
        const int width = (size == 8) ? 64 : 32;
        return std::format("{}varint{}", sign, width);
    }
    if (size == 1) {
        return std::format("{}int8", sign);
    }
    return std::format("{}int{}{}", sign, size * 8, big_endian ? "_be" : "");
}

std::string repeat_type(cereal::SerializationTraits traits)
{
    const bool no_size_compression = !!(traits & cereal::SerializationTraits::NoSizeCompression);
    const bool big_endian = !!(traits & cereal::SerializationTraits::BigEndian);
    if (no_size_compression) {
        return big_endian ? "uint32_be" : "uint32";
    }
    return "uvarint32";
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
    auto *payload = std::get_if<Type>(&types_.at(read_as.id()));
    if (!payload) {
        throw std::runtime_error("invalid state: packet payload is not visited");
    }
    payload->no_output = true;
    pk.payload = *payload;
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

void Visitor::visitType(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    // std::println("Type: {}", type.info().name());
    Type ty;
    ty.name = sanitise_typename(type);
    auto members = type.data();
    ty.fields.reserve(std::ranges::size(members));
    for (const auto &[id, data] : type.data()) {
        cereal::internal::BasicSchema::MemberDescriptor *descriptor = data.custom();
        if (!descriptor) {
            throw std::runtime_error("type member missing type descriptor");
        }
        ty.fields.emplace_back(buildField(data));
    }
    types_[type.id()] = std::move(ty);
}

FieldType Visitor::buildField(const entt::meta_data &data)
{
    cereal::internal::BasicSchema::MemberDescriptor *descriptor = data.custom();
    if (!descriptor) {
        throw std::runtime_error("type member missing type descriptor");
    }

    auto type = data.type();
    const auto traits = descriptor->mSerializationTraits;

    bool optional = false;
    for (;;) {
        if (!type.is_template_specialization()) {
            break;
        }
        auto tpl = type.template_type();
        if (tpl == entt::resolve<entt::meta_class_template_tag<std::optional>>(meta_ctx_)) {
            optional = true;
            type = type.template_arg(0);
        }
        else if (tpl == entt::resolve<entt::meta_class_template_tag<std::unique_ptr>>(meta_ctx_) ||
                 tpl == entt::resolve<entt::meta_class_template_tag<std::shared_ptr>>(meta_ctx_)) {
            type = type.template_arg(0);
        }
        else {
            break;
        }
    }

    if (type.is_enum()) {
        EnumField f;
        f.name = descriptor->mName;
        f.optional = optional;
        visit(type);
        f.enum_type = getTypeRef(type);
        if (!!(traits & cereal::SerializationTraits::EnumAsValue)) {
            f.type = serialization_type(type, traits);
        }
        else {
            f.type = std::string("string");
        }
        return f;
    }

    return std::visit(
        [&](auto &&spec) -> FieldType {
            using T = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<ArraySpec>>) {
                ArrayField f;
                f.name = descriptor->mName;
                f.optional = optional;
                f.repeat = std::move(spec->repeat);
                f.element_type = std::move(spec->element_type);
                return f;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<MapSpec>>) {
                MapField f;
                f.name = descriptor->mName;
                f.optional = optional;
                f.key_type = std::move(spec->key_type);
                f.value_type = std::move(spec->value_type);
                return f;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<VariantSpec>>) {
                VariantField f;
                f.name = descriptor->mName;
                f.optional = optional;
                f.switch_on = std::move(spec->switch_on);
                f.cases = std::move(spec->cases);
                return f;
            }
            else {
                Field f;
                f.name = descriptor->mName;
                f.optional = optional;
                f.type = std::forward<decltype(spec)>(spec);
                return f;
            }
        },
        buildTypeSpec(type, traits));
}

TypeSpec Visitor::buildTypeSpec(entt::meta_type type, cereal::SerializationTraits traits)
{
    if (type.is_template_specialization()) {
        auto tpl = type.template_type();
        if (tpl == entt::resolve<entt::meta_class_template_tag<std::optional>>(meta_ctx_)) {
            throw std::runtime_error(
                std::format("std::optional inside a container element is not supported ({})", type.info().name()));
        }
        if (tpl == entt::resolve<entt::meta_class_template_tag<std::unique_ptr>>(meta_ctx_) ||
            tpl == entt::resolve<entt::meta_class_template_tag<std::shared_ptr>>(meta_ctx_)) {
            type = type.template_arg(0);
        }
    }

    if (type.is_template_specialization() &&
        type.template_type() == entt::resolve<entt::meta_class_template_tag<std::variant>>(meta_ctx_)) {
        auto spec = std::make_shared<VariantSpec>();
        if (cereal::internal::BasicSchema::TaggedVariantDescriptor *tag = type.custom()) {
            auto tag_type = tag->mResolve(meta_ctx_);
            visit(tag_type);
            spec->switch_on = getTypeRef(tag_type);
        }
        else {
            spec->switch_on = repeat_type(traits);
        }
        for (auto i = 0; i < type.template_arity(); ++i) {
            auto c = type.template_arg(i);
            visit(c);
            spec->cases.emplace_back(getTypeRef(c));
        }
        return spec;
    }

    if (type.is_sequence_container()) {
        auto instance = type.construct();
        if (!instance) {
            throw std::runtime_error(
                std::format("cannot introspect non-default-constructible sequence {}", type.info().name()));
        }
        auto seq = instance.as_sequence_container();
        auto element_type = seq.value_type();
        if (!element_type) {
            throw std::runtime_error(std::format("invalid array element type ({})", type.info().name()));
        }
        auto spec = std::make_shared<ArraySpec>();
        if (const auto extent = seq.size(); extent > 0) {
            spec->repeat = static_cast<std::uint64_t>(extent);
        }
        else {
            spec->repeat = repeat_type(traits);
        }
        spec->element_type = buildTypeSpec(element_type, traits);
        return spec;
    }

    if (type.is_associative_container()) {
        auto instance = type.construct();
        if (!instance) {
            throw std::runtime_error(
                std::format("cannot introspect non-default-constructible associative {}", type.info().name()));
        }
        auto assoc = instance.as_associative_container();
        auto key_type = assoc.key_type();
        auto value_type = assoc.mapped_type();
        if (!key_type || !value_type) {
            throw std::runtime_error(std::format("invalid map key/value type ({})", type.info().name()));
        }
        auto spec = std::make_shared<MapSpec>();
        spec->key_type = buildTypeSpec(key_type, traits);
        spec->value_type = buildTypeSpec(value_type, traits);
        return spec;
    }

    if (type.is_integral()) {
        return TypeRef{serialization_type(type, traits)};
    }

    visit(type);
    return getTypeRef(type);
}

bool Visitor::isVisited(const entt::meta_type &type) const
{
    return types_.contains(type.id());
}

TypeRef Visitor::getTypeRef(const entt::meta_type &type) const
{
    auto it = types_.find(type.id());
    if (it == types_.end()) {
        return sanitise_typename(type);
    }
    return std::visit(
        [&](auto &&arg) -> TypeRef {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Packet>) {
                throw std::runtime_error("cannot reference a packet type");
            }
            else {
                return arg;
            }
        },
        it->second);
}

}  // namespace proto
