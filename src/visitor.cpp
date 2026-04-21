#include "visitor.h"

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

    auto desc = descriptor->mPtr->description(reflection_ctx_.internal(), config_);
    if (desc.mEnumValues && !desc.mEnumValues->empty()) {
        visitEnum(type, desc);
        return;
    }

    visitType(type, desc);
}

void Visitor::visitPacket(const entt::meta_type &type)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    std::println("Packet: {}", type.info().name());
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

    std::println("TypeAlias: using {} = {}", type.info().name(), value.info().name());
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

void Visitor::visitEnum(const entt::meta_type &type, const cereal::SchemaDescription &desc)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    std::println("Enum: {}", type.info().name());
    Enum en;
    en.name = sanitise_typename(type);
    if (desc.mEnumValues) {
        en.values.reserve(desc.mEnumValues->size());
        for (const auto &ev : *desc.mEnumValues) {
            en.values.emplace_back(ev.mName, ev.mValue);
        }
    }
    types_[type.id()] = std::move(en);
}

void Visitor::visitType(const entt::meta_type &type, const cereal::SchemaDescription &desc)
{
    if (isVisited(type)) {
        return;  // already visited
    }

    std::println("Type: {}", type.info().name());
    Type ty;
    ty.name = sanitise_typename(type);
    if (desc.mMembers) {
        ty.fields.reserve(desc.mMembers->size());
        for (const auto &[name, member] : *desc.mMembers) {
            std::println("  Field: {} - {}, {}, {}", name, static_cast<unsigned int>(member.mId),
                         member.mName.value_or(""), member.mDescription.value_or(""));
            Field f;
            f.name = name;
            f.deprecated = member.mDeprecated;
            if (member.mDescription) {
                f.description = trim(*member.mDescription);
            }

            // Type ref;
            // ref.name = sanitise_typename(entt::resolve(meta_ctx_, member.mId));
            // f.type = std::move(ref);
            ty.fields.emplace_back(std::move(f));
        }
    }
    types_[type.id()] = std::move(ty);
}

bool Visitor::isVisited(const entt::meta_type &type) const
{
    return types_.contains(type.id());
}

}  // namespace proto
