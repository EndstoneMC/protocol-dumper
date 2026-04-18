#include "json_writer.h"

#include <cctype>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace proto {

using Json = nlohmann::ordered_json;

namespace {

std::string sanitizeName(const std::string &raw)
{
    std::string result;
    result.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            result += c;
        }
        else if (c == ':' || c == ' ' || c == '-') {
            result += '_';
        }
    }
    if (result.empty()) {
        result = "unknown";
    }
    return result;
}

std::string stripTemplateArgs(const std::string &name)
{
    if (auto pos = name.find('<'); pos != std::string::npos) {
        return name.substr(0, pos);
    }
    return name;
}

Json typeSpecToJson(const TypeSpec &ts)
{
    if (auto *s = std::get_if<std::string>(&ts)) {
        return Json(*s);
    }
    if (auto *m = std::get_if<MapType>(&ts)) {
        Json j = Json::object();
        j["key"] = m->key;
        j["value"] = m->value;
        return j;
    }
    if (auto *v = std::get_if<VariantType>(&ts)) {
        Json j = Json::object();
        Json sw = Json::object();
        if (!v->switch_type.empty()) sw["type"] = v->switch_type;
        if (!v->switch_name.empty()) sw["name"] = v->switch_name;
        if (v->switch_enum) sw["enum"] = *v->switch_enum;
        if (!sw.empty()) j["switch"] = std::move(sw);
        Json cases = Json::array();
        for (const auto &c : v->cases) {
            cases.push_back(c.empty() ? Json() : Json(c));
        }
        j["cases"] = std::move(cases);
        return j;
    }
    return Json();
}

Json constraintsToJson(const Constraints &c)
{
    Json j = Json::object();
    if (c.mMinimum) j["minimum"] = *c.mMinimum;
    if (c.mMaximum) j["maximum"] = *c.mMaximum;
    if (c.mMinLength) j["min_length"] = *c.mMinLength;
    if (c.mMaxLength) j["max_length"] = *c.mMaxLength;
    if (c.mMinItems) j["min_items"] = *c.mMinItems;
    if (c.mMaxItems) j["max_items"] = *c.mMaxItems;
    if (c.mPattern) j["pattern"] = *c.mPattern;
    return j;
}

Json fieldToJson(const Field &f)
{
    Json j = Json::object();
    j["name"] = f.name;

    j["type"] = typeSpecToJson(f.type);
    if (!f.enum_name.empty()) {
        j["enum"] = f.enum_name;
    }
    if (!f.repeat.empty()) {
        j["repeat"] = f.repeat;
    }
    if (f.optional) {
        j["optional"] = true;
    }
    if (f.deprecated) {
        j["deprecated"] = true;
    }
    if (!f.description.empty()) {
        j["description"] = f.description;
    }
    if (f.constraints && !f.constraints->empty()) {
        j["constraints"] = constraintsToJson(*f.constraints);
    }
    return j;
}

Json enumEntryToJson(const EnumEntry &e)
{
    Json j = Json::object();
    j["name"] = e.name;
    j["value"] = e.value;
    if (!e.description.empty()) {
        j["description"] = e.description;
    }
    return j;
}

Json typeDefToJson(const TypeDef &td)
{
    Json j = Json::object();
    j["name"] = td.name;
    if (auto *fields = std::get_if<std::vector<Field>>(&td.body)) {
        j["kind"] = "struct";
        Json arr = Json::array();
        for (const auto &f : *fields) {
            arr.push_back(fieldToJson(f));
        }
        j["fields"] = std::move(arr);
    }
    else if (auto *entries = std::get_if<std::vector<EnumEntry>>(&td.body)) {
        j["kind"] = "enum";
        Json arr = Json::array();
        for (const auto &e : *entries) {
            arr.push_back(enumEntryToJson(e));
        }
        j["values"] = std::move(arr);
    }
    return j;
}

Json packetToJson(const Packet &pkt)
{
    Json j = Json::object();
    j["id"] = pkt.id;
    j["name"] = pkt.name;
    if (!pkt.description.empty()) {
        j["description"] = pkt.description;
    }
    Json fields = Json::array();
    for (const auto &f : pkt.fields) {
        fields.push_back(fieldToJson(f));
    }
    j["fields"] = std::move(fields);
    return j;
}

}  // namespace

void write_json(const std::vector<Packet> &packets,
                const std::vector<TypeDef> &types,
                const std::filesystem::path &output_dir)
{
    const auto packets_dir = output_dir / "packets";
    const auto types_dir = output_dir / "types";
    std::filesystem::create_directories(packets_dir);
    std::filesystem::create_directories(types_dir);

    for (const auto &pkt : packets) {
        const auto filename = sanitizeName(pkt.name) + ".json";
        std::ofstream out(packets_dir / filename);
        out << packetToJson(pkt).dump(2) << "\n";
    }

    std::set<std::string> emitted;
    for (const auto &td : types) {
        const auto base = stripTemplateArgs(td.name);
        const auto filename = sanitizeName(base) + ".json";
        if (!emitted.insert(filename).second) {
            continue;
        }

        auto j = typeDefToJson(td);
        j["name"] = base;
        std::ofstream out(types_dir / filename);
        out << j.dump(2) << "\n";
    }
}

}  // namespace proto
