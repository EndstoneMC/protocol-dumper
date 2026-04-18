#pragma once

#include <string>
#include <string_view>

namespace proto {

// Strips "struct ", "class ", "enum ", "union " prefixes.
// Loops to handle compound forms like "enum class Foo".
inline std::string_view stripTypePrefix(std::string_view name)
{
    constexpr std::string_view kPrefixes[] = {"struct ", "class ", "enum ", "union "};
    for (bool changed = true; changed;) {
        changed = false;
        for (auto p : kPrefixes) {
            if (name.starts_with(p)) {
                name.remove_prefix(p.size());
                changed = true;
                break;
            }
        }
    }
    return name;
}

// Removes all occurrences of "(anonymous namespace)::" from a type name.
inline std::string stripAnonymousNamespace(std::string name)
{
    constexpr std::string_view kAnon = "(anonymous namespace)::";
    for (std::string::size_type pos; (pos = name.find(kAnon)) != std::string::npos;) {
        name.erase(pos, kAnon.size());
    }
    return name;
}

inline std::string_view trim(std::string_view s)
{
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return {};
    return s.substr(start, s.find_last_not_of(" \t\n\r") - start + 1);
}

}  // namespace proto
