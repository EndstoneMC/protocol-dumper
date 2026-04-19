#include "json_writer.h"

#include <cctype>
#include <fstream>

namespace proto {

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

}  // namespace

JsonWriter::JsonWriter(std::filesystem::path output_dir) : output_dir_(std::move(output_dir))
{
    std::filesystem::create_directories(output_dir_ / "packets");
    std::filesystem::create_directories(output_dir_ / "types");
}

void JsonWriter::write(const Packet &pkt)
{
    json doc = json::object();
    doc["id"] = pkt.id;
    doc["name"] = pkt.name;
    if (!pkt.description.empty()) {
        doc["description"] = pkt.description;
    }
    doc["fields"] = json::array();

    scope_.push_back(&doc["fields"]);
    for (const auto &f : pkt.fields) {
        write(f);
    }
    scope_.pop_back();

    const auto filename = sanitizeName(pkt.name) + ".json";
    std::ofstream out(output_dir_ / "packets" / filename);
    out << doc.dump(2) << "\n";
}

void JsonWriter::write(const TypeDef &td)
{
    const auto base = stripTemplateArgs(td.name);
    const auto filename = sanitizeName(base) + ".json";
    if (!emitted_types_.insert(filename).second) {
        return;
    }

    json doc = json::object();
    doc["name"] = base;

    if (auto *fields = std::get_if<std::vector<Field>>(&td.body)) {
        doc["kind"] = "struct";
        doc["fields"] = json::array();
        scope_.push_back(&doc["fields"]);
        for (const auto &f : *fields) {
            write(f);
        }
        scope_.pop_back();
    }
    else if (auto *entries = std::get_if<std::vector<EnumEntry>>(&td.body)) {
        doc["kind"] = "enum";
        doc["values"] = json::array();
        scope_.push_back(&doc["values"]);
        for (const auto &e : *entries) {
            write(e);
        }
        scope_.pop_back();
    }

    std::ofstream out(output_dir_ / "types" / filename);
    out << doc.dump(2) << "\n";
}

void JsonWriter::write(const Field &f)
{
    json j = json::object();
    j["name"] = f.name;

    scope_.push_back(&j);
    write(f.type);
    scope_.pop_back();

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
        scope_.push_back(&j);
        write(*f.constraints);
        scope_.pop_back();
    }

    scope_.back()->push_back(std::move(j));
}

void JsonWriter::write(const EnumEntry &e)
{
    json j = json::object();
    j["name"] = e.name;
    j["value"] = e.value;
    if (!e.description.empty()) {
        j["description"] = e.description;
    }
    scope_.back()->push_back(std::move(j));
}

void JsonWriter::write(const Constraints &c)
{
    json j = json::object();
    if (c.minimum) {
        j["minimum"] = *c.minimum;
    }
    if (c.maximum) {
        j["maximum"] = *c.maximum;
    }
    if (c.min_length) {
        j["min_length"] = *c.min_length;
    }
    if (c.max_length) {
        j["max_length"] = *c.max_length;
    }
    if (c.min_items) {
        j["min_items"] = *c.min_items;
    }
    if (c.max_items) {
        j["max_items"] = *c.max_items;
    }
    if (c.pattern) {
        j["pattern"] = *c.pattern;
    }
    (*scope_.back())["constraints"] = std::move(j);
}

void JsonWriter::write(const TypeSpec &ts)
{
    json &parent = *scope_.back();
    if (auto *s = std::get_if<std::string>(&ts)) {
        parent["type"] = *s;
        return;
    }
    if (auto *m = std::get_if<MapType>(&ts)) {
        json j = json::object();
        j["key"] = m->key;
        j["value"] = m->value;
        parent["type"] = std::move(j);
        return;
    }
    if (auto *v = std::get_if<VariantType>(&ts)) {
        json j = json::object();
        json sw = json::object();
        if (!v->switch_type.empty()) {
            sw["type"] = v->switch_type;
        }
        if (!v->switch_name.empty()) {
            sw["name"] = v->switch_name;
        }
        if (v->switch_enum) {
            sw["enum"] = *v->switch_enum;
        }
        if (!sw.empty()) {
            j["switch"] = std::move(sw);
        }
        json cases = json::array();
        for (const auto &c : v->cases) {
            cases.push_back(c.empty() ? json() : json(c));
        }
        j["cases"] = std::move(cases);
        parent["type"] = std::move(j);
        return;
    }
}

}  // namespace proto
