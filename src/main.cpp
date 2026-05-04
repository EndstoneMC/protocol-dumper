#include <unistd.h>

#include <climits>
#include <filesystem>
#include <fstream>
#include <print>

#include <funchook.h>
#include <libhat.hpp>

#include "cereal/Context.h"
#include "common/network/packet/cerealize/core/PacketSerializationHelper.h"
#include "visitor.h"

namespace {
template <class... Ts>
struct overloads : Ts... {
    using Ts::operator()...;
};

void dump(const cereal::ReflectionCtx &ctx)
{
    char exe_path[PATH_MAX];
    auto len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    exe_path[len > 0 ? len : 0] = '\0';
    auto exe_dir = std::filesystem::path(exe_path).parent_path();
    auto output_dir = exe_dir / "data" / "protocol";
    std::filesystem::create_directories(output_dir);

    proto::Visitor visitor(ctx);
    std::size_t packets = 0, enums = 0, types = 0;
    for (const auto &type : visitor.getTypes() | std::views::values) {
        std::visit(
            overloads{
                [&](auto &&arg) {
                    using T = std::decay_t<decltype(arg)>;
                    auto path = output_dir;
                    if constexpr (std::is_same_v<T, proto::TypeAlias>) {
                        return;
                    }
                    else if constexpr (std::is_same_v<T, proto::Packet>) {
                        path /= "packets";
                        ++packets;
                    }
                    else if constexpr (std::is_same_v<T, proto::Enum>) {
                        path /= "enums";
                        ++enums;
                    }
                    else {
                        if (arg.no_output) {
                            return;
                        }
                        path /= "types";
                        ++types;
                    }
                    create_directories(path);
                    auto filename = arg.name;
                    std::ranges::replace(filename, ':', '_');
                    std::ranges::replace(filename, '<', '_');
                    std::ranges::replace(filename, '>', '_');
                    auto file_path = path / (filename + ".json");
                    std::ofstream f(file_path);
                    f << nlohmann::ordered_json(arg).dump(2);
                    if (!f) {
                        std::println(stderr, "!!! ERROR: failed to write {}", file_path.string());
                    }
                },
            },
            type);
    }
    std::println("Dumped {} packets, {} enums, {} types to {}", packets, enums, types, output_dir.string());
}

using BindPacketsFn = void (*)(cereal::ReflectionCtx &);
BindPacketsFn original_bindPackets = nullptr;

void hooked_bindPackets(cereal::ReflectionCtx &ctx)
{
    original_bindPackets(ctx);
    try {
        dump(ctx);
    }
    catch (const std::exception &e) {
        std::println(stderr, "!!! FATAL: {}", e.what());
    }
}

void install_hook()
{
    auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 49 8B 55 ? 48 8B 72">(), ".text");
    if (!result.has_result()) {
        throw std::runtime_error("Sigscan failed: PacketSerialization::bindPackets");
    }
    original_bindPackets = reinterpret_cast<BindPacketsFn>(result.rel(1));

    auto *hook = funchook_create();
    if (!hook) {
        throw std::runtime_error("funchook_create failed");
    }
    if (auto rc = funchook_prepare(hook, reinterpret_cast<void **>(&original_bindPackets),
                                   reinterpret_cast<void *>(&hooked_bindPackets));
        rc != 0) {
        throw std::runtime_error(std::string("funchook_prepare: ") + funchook_error_message(hook));
    }
    if (auto rc = funchook_install(hook, 0); rc != 0) {
        throw std::runtime_error(std::string("funchook_install: ") + funchook_error_message(hook));
    }
}

__attribute__((constructor)) void main()
{
    try {
        install_hook();
    }
    catch (const std::exception &e) {
        std::println(stderr, "!!! FATAL: {}", e.what());
    }
}
}  // namespace
