#include "json_writer.h"

#include <cctype>
#include <fstream>
#include <set>
#include <string>

namespace proto::output {

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

void JsonWriter::write(const std::vector<model::Packet> &packets,
                       const std::vector<std::unique_ptr<model::Class>> &classes,
                       const std::filesystem::path &output_dir) const
{
    const auto packets_dir = output_dir / "packets";
    const auto types_dir = output_dir / "types";
    std::filesystem::create_directories(packets_dir);
    std::filesystem::create_directories(types_dir);

    for (const auto &pkt : packets) {
        const auto filename = sanitizeName(pkt.mName) + ".json";
        std::ofstream out(packets_dir / filename);
        out << pkt.toJson().dump(2) << "\n";
    }

    std::set<std::string> emitted;
    for (const auto &cls : classes) {
        if (!cls) continue;
        const auto base = stripTemplateArgs(cls->mName);
        const auto filename = sanitizeName(base) + ".json";
        if (!emitted.insert(filename).second) {
            continue;
        }

        auto j = cls->toJson();
        j["name"] = base;
        std::ofstream out(types_dir / filename);
        out << j.dump(2) << "\n";
    }
}

}  // namespace proto::output
