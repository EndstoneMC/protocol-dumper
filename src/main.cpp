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
#include "version.h"
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
    // Each pattern is anchored on the lone CALL to PacketSerialization::bindPackets, and only the LATEST
    // preview build of an update is a supported target - builds within one update disagree here (the
    // 1.26.0.24 call site matches neither pattern below; 1.26.0.29, the build we target, matches the
    // first). Stable builds usually ride the pattern of the update they branched from - every 1.26 one
    // checked does, 1.26.0.2 .. 1.26.44.3 - but not always: 1.21.132.3 matches neither 1.21.130 pattern
    // and needs its own, below. 1.21.90.28 and older are a hard floor whatever we
    // do here: they hold zero references to "[cereal:packet]" and their NetworkSystem ctor has no
    // bindPackets call at all, so there is no schema graph to walk. To cut a pattern for a new BDS,
    // locate that call site from scratch:
    // 1. find bytes "50 61 63 6B 65 74 20 52 65 63 65 69 76 65 72 00" = "Packet Receiver\0"; it has exactly
    //    one rip-relative xref in .text on every version checked (1.21.130 .. 1.26.50), a `lea reg, [rip+d]`
    //    inside the packet-registration function that calls bindPackets and then names the receiver
    // 2. walk back from that lea to the call. It is the nearest preceding E8 on the builds checked, ~13
    //    bytes above the lea, but the shape between them is `mov r, [r2]; mov rdi, r; call getCoroutinePool`
    //    whose register allocation churns per build, so never assume the delta - decode each candidate E8 in
    //    the window and confirm by step 3
    // 3. confirm the E8 target IS bindPackets: it opens `53 48 89 FB` (push rbx; mov rbx,rdi - rbx holds the
    //    ReflectionCtx&) and its whole body is a flat run of `call cerealizer<T>::bind; mov rdi,rbx` pairs
    //    with nothing else - 165 pairs on 1.21.130 growing to 230 on 1.26.50.25, i.e. one per bound packet.
    //    This step is not optional: the pre-1.26 pattern still matches once in 1.26.0.29, where it resolves
    //    a `41 57 41 56 ...` prologue that is plainly not bindPackets
    // 4. cross-check the closure: those callees are the per-packet bind functions, so they reference
    //    "[cereal:packet]" / "[cereal:packet_details]" and their own packet-name string. Any candidate whose
    //    callees reach none of those is the wrong function
    // 5. cut the pattern starting at the E8 so .rel(1) resolves it, and verify it is unique in .text
#if BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 26, 0, 0)
    auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? ? 8B 55 ? 48 8B 72">(), ".text");
#elif BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 21, 132, 0)
    auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 49 8B 2E 48 89 EF">(), ".text");
#elif BEDROCK_SERVER_VERSION_HEX >= BEDROCK_SERVER_VERSION_ENCODE(1, 21, 130, 0)
    auto result = hat::find_pattern(hat::compile_signature<"E8 ? ? ? ? 48 8B 6D ? 48 89 EF">(), ".text");
#else
// Groundwork for dropping the floor to 1.21.100, where cereal packet binding first appears. These
// signatures are cut and verified - each is a unique .text hit resolving a bindPackets whose leading
// callees all reference "[cereal:packet]" - and the hook does arm and fire with them:
//   1.21.120.x  "E8 ? ? ? ? 48 8B 5D 00 48 89 DF E8 AB"   -> bindPackets, 148 binds
//   1.21.110.x  "E8 ? ? ? ? 49 8B 2E 48 89 EF"            -> bindPackets, 137 binds
//   1.21.100.x  "E8 ? ? ? ? 49 8B 5D 00 48 89"            -> bindPackets, partial cereal migration
// What still blocks them is the entt ABI, not the scan. Those builds predate two entt changes:
// meta_type_node still carries `resolve` and `dtor` and holds `details` by shared_ptr, and basic_any
// keeps its storage inline rather than in a basic_any_storage base. entt 182a6d5f matches both, and
// pinning it moves the crash from Visitor::visit to Visitor::visitPacket - so the meta nodes then read
// correctly - but the UserPropertiesMap value still comes back wrong (null resolver, empty any, and the
// packet id nowhere in the node), so at least one more container layout is off. Note the r21_u1x
// bedrock-headers are the Android arm64 build; the Linux server may not match them.
#error "1.21.130 is the floor; older updates need the pre-1.21.130 entt ABI sorted out first"
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
