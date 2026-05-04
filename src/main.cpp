#include <funchook.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <libhat.hpp>
#include <libhat_linux.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "cereal/Context.h"
#include "common/network/packet/cerealize/core/PacketSerializationHelper.h"
#include "visitor.h"

namespace {
template <class... Ts>
struct overloads : Ts... {
    using Ts::operator()...;
};

void init_logger()
{
    auto logger = spdlog::stdout_color_mt("protocol-dumper");
    logger->set_pattern("[%n] [%^%l%$] %v");
#ifndef NDEBUG
    logger->set_level(spdlog::level::debug);
#endif
    spdlog::set_default_logger(std::move(logger));
}

void dump(const cereal::ReflectionCtx &ctx)
{
    char exe_path[PATH_MAX];
    auto len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    exe_path[len > 0 ? len : 0] = '\0';
    auto exe_dir = std::filesystem::path(exe_path).parent_path();
    auto output_dir = exe_dir / "data" / "protocol";
    std::filesystem::create_directories(output_dir);

    proto::Visitor visitor(ctx);
    const auto &types = visitor.getTypes();
    spdlog::info("dumping {} reflected types to {}", types.size(), output_dir.string());

    const auto start = std::chrono::steady_clock::now();
    std::size_t packet_count = 0, enum_count = 0, type_count = 0, skipped = 0, write_errors = 0;
    for (const auto &type : types | std::views::values) {
        std::visit(
            overloads{
                [&](auto &&arg) {
                    using T = std::decay_t<decltype(arg)>;
                    auto path = output_dir;
                    if constexpr (std::is_same_v<T, proto::TypeAlias>) {
                        ++skipped;
                        return;
                    }
                    else if constexpr (std::is_same_v<T, proto::Packet>) {
                        path /= "packets";
                        ++packet_count;
                    }
                    else if constexpr (std::is_same_v<T, proto::Enum>) {
                        path /= "enums";
                        ++enum_count;
                    }
                    else {
                        if (arg.no_output) {
                            ++skipped;
                            return;
                        }
                        path /= "types";
                        ++type_count;
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
                        spdlog::warn("failed to write {}", file_path.string());
                        ++write_errors;
                    }
                },
            },
            type);
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    spdlog::info("dump complete: {} packets, {} enums, {} types ({} skipped) in {} ms", packet_count, enum_count,
                 type_count, skipped, elapsed.count());
    if (write_errors > 0) {
        spdlog::warn("{} file(s) failed to write", write_errors);
    }
}

using BindPacketsFn = decltype(&PacketSerialization::bindPackets);
BindPacketsFn original_bindPackets = nullptr;

void hooked_bindPackets(cereal::ReflectionCtx &ctx)
{
    spdlog::info("PacketSerialization::bindPackets called, invoking original");
    original_bindPackets(ctx);
    try {
        dump(ctx);
    }
    catch (const std::exception &e) {
        spdlog::error("dump failed: {}", e.what());
    }
}

void install_hook()
{
    // search for 50 61 63 6B 65 74 20 52 65 63 65 69 76 65 72 00, then xref, the function is right above
#if BEDROCK_SERVER_VERSION_MINOR >= 26
    auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? ? 8B 55 ? 48 8B 72">(), ".text");
#else
    auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 48 8B 6D ? 48 89 EF">(), ".text");
#endif
    if (!result.has_result()) {
        throw std::runtime_error("sigscan failed for PacketSerialization::bindPackets");
    }
    auto *target = result.rel(1);
    spdlog::info("resolved PacketSerialization::bindPackets at {}", static_cast<const void *>(target));
    original_bindPackets = reinterpret_cast<BindPacketsFn>(target);

    auto *hook = funchook_create();
    if (!hook) {
        throw std::runtime_error("funchook_create returned null");
    }
    if (auto rc = funchook_prepare(hook, reinterpret_cast<void **>(&original_bindPackets),
                                   reinterpret_cast<void *>(&hooked_bindPackets));
        rc != 0) {
        throw std::runtime_error(std::format("funchook_prepare failed: {}", funchook_error_message(hook)));
    }
    if (auto rc = funchook_install(hook, 0); rc != 0) {
        throw std::runtime_error(std::format("funchook_install failed: {}", funchook_error_message(hook)));
    }
    spdlog::info("hook armed, awaiting PacketSerialization::bindPackets");
}
}  // namespace

__attribute__((constructor)) void on_load()
{
    init_logger();
    spdlog::info("loaded, installing PacketSerialization::bindPackets hook");
    try {
        install_hook();
    }
    catch (const std::exception &e) {
        spdlog::error("hook installation failed: {}", e.what());
    }
}
