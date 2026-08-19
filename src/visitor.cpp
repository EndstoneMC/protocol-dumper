#include "visitor.h"

#include <limits>
#include <print>
#include <ranges>

#include <spdlog/spdlog.h>

#include "cereal/schema/BasicSchema.h"
#include "common/network/packet/cerealize/core/PacketSerializationHelper.h"

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

// entt renamed meta_type::id() to alias() mid-4.0; BDS pins differ by update.
template <typename T>
entt::id_type type_key(const T &type)
{
    if constexpr (requires { type.id(); }) {
        return type.id();
    }
    else {
        return type.alias();
    }
}

std::string trim(std::string_view s)
{
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return {};
    }
    return std::string(s.substr(start, s.find_last_not_of(" \t\n\r") - start + 1));
}

// `is_signed` is `std::is_signed_v<Type>`, false for every enum since an enum is not
// arithmetic, and BDS stamped that bit with its own compiled entt. Its conversion
// helper is the only place the underlying type survives, so read an all-ones object
// back through it: signed yields -1, unsigned 2^bits - 1. uint64 storage covers the
// size and alignment of every underlying, and all-ones is endian-independent.
bool underlying_is_signed(const entt::meta_type &type)
{
    const std::uint64_t all_ones = ~std::uint64_t{0};
    auto probe = type.from_void(static_cast<const void *>(&all_ones));
    if (!probe || !probe.allow_cast<double>()) {
        throw std::runtime_error(std::format("cannot probe the sign of enum {}", type.info().name()));
    }
    return probe.cast<double>() < 0.0;
}

std::string serialization_type(const entt::meta_ctx &ctx, const entt::meta_type &type,
                               cereal::SerializationTraits traits)
{
    if (type == entt::resolve<bool>(ctx)) {
        return std::string(type.info().name());
    }

    const auto size = type.size_of();
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        throw std::runtime_error(std::format("unsupported enum underlying size {} in {}", size, type.info().name()));
    }

    const bool is_signed = type.is_enum() ? underlying_is_signed(type) : type.is_signed();

    const bool compression = !!(traits & cereal::SerializationTraits::Compression);
    const bool big_endian = !!(traits & cereal::SerializationTraits::BigEndian);
    const std::string_view sign = is_signed ? "" : "u";

    // A byte has nothing to compress, and cereal reaches one through SchemaWriter's own
    // write(int8_t) / write(uint8_t) overload, which has no varint path. BDS sets the
    // trait anyway, so honouring it here made a varint32 of InventorySource's container
    // id -- a signed char, so every non-zero value zigzagged to the wrong bytes.
    if (size == 1) {
        return std::format("{}int8", sign);
    }
    if (compression) {
        const int width = (size == 8) ? 64 : 32;
        return std::format("{}varint{}", sign, width);
    }
    return std::format("{}int{}{}", sign, size * 8, big_endian ? "_be" : "");
}

// A fixed-extent array of bytes stopped being an array at 1.26.50.25: cereal binds it to
// the blob path (SchemaWriter::write(gsl::span<const unsigned char>)) instead of
// openArray + per-element write, so a uvarint length now precedes the bytes. That path
// takes no isDynamicExtent and ignores NoSizeCompression, so the prefix is always a
// uvarint32 -- SubChunkPacket's 16x16 heightmap gained one byte per row.
bool is_byte_blob([[maybe_unused]] const entt::meta_ctx &ctx, [[maybe_unused]] const entt::meta_type &element)
{
#if BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 26, 50, 25)
    return element == entt::resolve<char>(ctx) || element == entt::resolve<signed char>(ctx) ||
           element == entt::resolve<unsigned char>(ctx);
#else
    return false;
#endif
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

bool is_default_setter(const entt::meta_func &func)
{
    return static_cast<std::uint16_t>(func.traits<cereal::internal::MemberTraits>()) &
           static_cast<std::uint16_t>(cereal::internal::MemberTraits::isDefaultSetter);
}

// Binary wire representation of a cereal serialize-as type. cereal::doSave invokes
// the getter, so the getter's return type is the write shape. Its same-typed
// isDefaultSetter setter is the read partner (used as the fallback when no getter
// is bound); the remaining setters are SkipAlsoReadAs read-only alternates (e.g.
// the CSS-hex color) and never touch the binary wire. Returns a null meta_type
// when the type binds no getter and no setter (i.e. it is not a serialize-as).
entt::meta_type serialize_as_rep(const entt::meta_type &type)
{
    entt::meta_type default_setter_arg, any_setter_arg;
    for (const auto &[id, head] : type.func()) {
        for (auto func = head; func; func = func.next()) {
            if (const cereal::internal::BasicSchema::GetterDescriptor *getter = func.custom()) {
                if (func.ret()) {
                    return func.ret();
                }
            }
            else if (const cereal::internal::BasicSchema::SetterDescriptor *setter = func.custom()) {
                if (func.arity() <= 0) {
                    continue;
                }
                if (is_default_setter(func) && !default_setter_arg) {
                    default_setter_arg = func.arg(0);
                }
                if (!any_setter_arg) {
                    any_setter_arg = func.arg(0);
                }
            }
        }
    }
    return default_setter_arg ? default_setter_arg : any_setter_arg;
}

}  // namespace

Visitor::Visitor(const cereal::ReflectionCtx &ctx) : reflection_ctx_(ctx), meta_ctx_(ctx.internal().mMetaCtx) {}

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

    if (entt::meta_type alias = serialize_as_rep(type)) {
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
    spdlog::debug("visitPacket: {}", type.info().name());
    const cereal::internal::BasicSchema::TypeDescriptor *descriptor = type.custom();
    if (!descriptor) {
        throw std::runtime_error("packet missing type descriptor");
    }
    auto it = descriptor->mUserPropertiesMap.find("[cereal:packet]");
    Packet pk;
    pk.id = entt::any_cast<int>(it->second.second);
    pk.name = sanitise_typename(type);
    it = descriptor->mUserPropertiesMap.find("[cereal:packet_details]");
    if (it != descriptor->mUserPropertiesMap.end()) {
        std::string_view notes = entt::any_cast<const char *>(it->second.second);
        if (!notes.empty()) {
            pk.notes = trim(notes);
        }
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
    // if (!read_as || read_as != write_as) {
    if (!write_as) {
        throw std::runtime_error("invalid packet payload");
    }
    visit(write_as);
    auto *payload = std::get_if<Type>(&types_.at(type_key(write_as)));
    if (!payload) {
        throw std::runtime_error("invalid state: packet payload is not visited");
    }
    payload->no_output = true;
    pk.payload = *payload;
    types_[type_key(type)] = std::move(pk);
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
    if (types_.contains(type_key(value))) {
        auto &ty = types_.at(type_key(value));
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
    types_[type_key(type)] = std::move(type_alias);
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
    types_[type_key(type)] = std::move(en);
}

void Visitor::visitType(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }
    spdlog::debug("visitType: {}", type.info().name());
    Type ty;
    ty.name = sanitise_typename(type);
    auto members = type.data();
    ty.fields.reserve(std::ranges::size(members));
    for (const auto &[id, data] : type.data()) {
        cereal::internal::BasicSchema::MemberDescriptor *descriptor = data.custom();
        if (!descriptor) {
            throw std::runtime_error("type member missing type descriptor");
        }

        // cereal writes a fixed bool ahead of an isKeyedSetterGetter member. 1.26.50.25
        // deleted that branch from doSave outright, so the bool is no longer on the wire.
#if BEDROCK_SERVER_VERSION_HEX < BEDROCK_SERVER_VERSION_ENCODE(1, 26, 50, 25)
        if (static_cast<std::uint16_t>(data.traits<cereal::internal::MemberTraits>()) &
            static_cast<std::uint16_t>(cereal::internal::MemberTraits::isKeyedSetterGetter)) {
            Field marker;
            marker.type = std::string("bool");
            marker.value = true;
            ty.fields.emplace_back(std::move(marker));
        }
#endif

        // Special treatment for TypeWrapper<T> - unwrap and inline
        if (data.type().is_template_specialization() &&
            data.type().template_type() == entt::resolve<entt::meta_class_template_tag<TypeWrapper>>(meta_ctx_)) {
            visit(data.type());
            auto &wrapper = std::get<Type>(types_.at(type_key(data.type())));
            wrapper.no_output = true;
            ty.fields.insert(ty.fields.end(), wrapper.fields.begin(), wrapper.fields.end());
        }
        else {
            ty.fields.emplace_back(buildField(data));
        }
    }
    types_[type_key(type)] = std::move(ty);
}

FieldType Visitor::buildField(const entt::meta_data &data)
{
    cereal::internal::BasicSchema::MemberDescriptor *descriptor = data.custom();
    if (!descriptor) {
        throw std::runtime_error("type member missing type descriptor");
    }

    // A conditionally-serialized member binds a type-erased serialize thunk, so
    // `data.type()` is that closure's signature rather than the payload type.
    // Its `mDynamicSetterArgCtor` resolves the real setter-argument type -- the
    // same resolver mechanism TaggedVariantDescriptor uses for its tag. 1.26.50.25
    // dropped the thunk and binds the payload type on the node, so there is
    // nothing left to resolve. (TaggedVariantDescriptor::mResolve is unaffected.)
#if BEDROCK_SERVER_VERSION_HEX < BEDROCK_SERVER_VERSION_ENCODE(1, 26, 50, 25)
    auto type = descriptor->mDynamicSetterArgCtor ? descriptor->mDynamicSetterArgCtor(meta_ctx_) : data.type();
#else
    auto type = data.type();
#endif
    const auto traits = descriptor->mSerializationTraits;

    bool optional = false;
    while (type.is_template_specialization()) {
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

    auto init = [&](auto &f) {
        f.name = descriptor->mName;
        f.optional = optional;
        f.deprecated = descriptor->mIsDeprecatedComponent;
        if (descriptor->mConstraint) {
            f.constraints = descriptor->mConstraint->doDescription(cereal::ContextArea::ALL);
        }
    };

    return std::visit(
        [&](auto &&spec) -> FieldType {
            using T = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<EnumSpec>>) {
                EnumField f;
                init(f);
                f.type = std::move(spec->type);
                f.enum_type = std::move(spec->enum_type);
                return f;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ArraySpec>>) {
                ArrayField f;
                init(f);
                f.repeat = std::move(spec->repeat);
                f.element_type = std::move(spec->element_type);
                return f;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<MapSpec>>) {
                MapField f;
                init(f);
                f.key_type = std::move(spec->key_type);
                f.value_type = std::move(spec->value_type);
                return f;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<VariantSpec>>) {
                VariantField f;
                init(f);
                f.switch_on = std::move(spec->switch_on);
                f.cases = std::move(spec->cases);
                return f;
            }
            else {
                Field f;
                init(f);
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

    if (type.is_enum()) {
        visit(type);
        auto spec = std::make_shared<EnumSpec>();
        spec->enum_type = getTypeRef(type);
        spec->type = !!(traits & cereal::SerializationTraits::EnumAsValue)
                         ? serialization_type(meta_ctx_, type, traits)
                         : std::string("string");
        return spec;
    }

    visit(type);
    if (auto it = types_.find(type_key(type)); it != types_.end() && std::holds_alternative<TypeAlias>(it->second)) {
        if (entt::meta_type rep = serialize_as_rep(type)) {
            return buildTypeSpec(rep, traits);
        }
    }

    if (type.is_template_specialization() &&
        type.template_type() == entt::resolve<entt::meta_class_template_tag<std::variant>>(meta_ctx_)) {
        auto spec = std::make_shared<VariantSpec>();
        if (cereal::internal::BasicSchema::TaggedVariantDescriptor *tag = type.custom()) {
            auto tag_type = tag->mResolve(meta_ctx_);
            visit(tag_type);
            spec->switch_on.type = serialization_type(meta_ctx_, tag_type, cereal::SerializationTraits::Compression);
            if (!tag->mTaggedName.empty()) {
                spec->switch_on.name = tag->mTaggedName;
            }
            if (tag_type.is_enum()) {
                spec->switch_on.enum_type = getTypeRef(tag_type);
            }
        }
        else {
            // doSavePlainVariant forces Compression on for the index write and restores the
            // member's traits before the alternative, so the index is always a uvarint32.
            spec->switch_on.type = "uvarint32";
        }
        for (auto i = 0; i < type.template_arity(); ++i) {
            spec->cases.emplace_back(buildTypeSpec(type.template_arg(i), traits));
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
        if (const auto extent = seq.size(); extent == 0) {
            spec->repeat = repeat_type(traits);
        }
        else if (is_byte_blob(meta_ctx_, element_type)) {
            spec->repeat = std::string("uvarint32");
        }
        else {
            spec->repeat = static_cast<std::uint64_t>(extent);
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
        if (!key_type) {
            throw std::runtime_error(std::format("invalid map key/value type ({})", type.info().name()));
        }
        // Key-only container (std::set / std::unordered_set): a count-prefixed list of keys.
        if (!value_type) {
            auto spec = std::make_shared<ArraySpec>();
            spec->repeat = repeat_type(traits);
            spec->element_type = buildTypeSpec(key_type, traits);
            return spec;
        }
        auto spec = std::make_shared<MapSpec>();
        spec->key_type = buildTypeSpec(key_type, traits);
        spec->value_type = buildTypeSpec(value_type, traits);
        return spec;
    }

    if (type.is_integral()) {
        return TypeRef{serialization_type(meta_ctx_, type, traits)};
    }

    visit(type);
    return getTypeRef(type);
}

bool Visitor::isVisited(const entt::meta_type &type) const
{
    return types_.contains(type_key(type));
}

TypeRef Visitor::getTypeRef(const entt::meta_type &type) const
{
    auto it = types_.find(type_key(type));
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
