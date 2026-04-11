#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <funchook.h>
#include <libhat.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "common/Packet.h"
#include "reader.h"
#include "generator.h"
#include "signatures.h"

// --- NetworkSystem::update hook to capture this pointer ---

using NetworkSystemUpdateFn = void (*)(void *self, const void *userList);
static NetworkSystemUpdateFn orig_update = nullptr;
static void *captured_network_system = nullptr;
static HANDLE capture_event = nullptr;

static void hooked_update(void *self, const void *userList)
{
    if (!captured_network_system) {
        captured_network_system = self;
        SetEvent(capture_event);
    }
    orig_update(self, userList);
}

// --- Signature scanning helper ---

template <hat::fixed_signature Sig>
static void *findSig(const char *name)
{
    auto mod = hat::process::get_process_module();
    auto result = hat::find_pattern<Sig>(mod.get());
    if (!result.has_result()) {
        spdlog::error("Signature not found: {}", name);
        return nullptr;
    }
    return const_cast<void *>(static_cast<const void *>(result.get()));
}

DWORD WINAPI DumpThread(LPVOID param)
{
    // Resolve output dir relative to the host executable
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::filesystem::path output_dir = std::filesystem::path(exe_path).parent_path() / "data" / "proto";
    std::filesystem::create_directories(output_dir);

    auto logger = spdlog::basic_logger_mt("dumper", (output_dir / "dump_log.txt").string(), true);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("=== BDS Packet Schema Dump ===");

    // --- Hook NetworkSystem::update to capture the instance ---

    auto *update_addr = findSig<sig::NETWORK_SYSTEM_UPDATE>("NetworkSystem::update");
    if (!update_addr) {
        spdlog::error("Cannot proceed without NetworkSystem::update");
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    capture_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    orig_update = reinterpret_cast<NetworkSystemUpdateFn>(update_addr);

    funchook_t *hook = funchook_create();
    int rv = funchook_prepare(hook, reinterpret_cast<void **>(&orig_update), reinterpret_cast<void *>(hooked_update));
    if (rv != 0) {
        spdlog::error("funchook_prepare failed: {}", funchook_error_message(hook));
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }
    rv = funchook_install(hook, 0);
    if (rv != 0) {
        spdlog::error("funchook_install failed: {}", funchook_error_message(hook));
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    spdlog::info("Hooked NetworkSystem::update, waiting for first call...");
    WaitForSingleObject(capture_event, INFINITE);
    CloseHandle(capture_event);

    funchook_uninstall(hook, 0);
    funchook_destroy(hook);

    spdlog::info("Captured NetworkSystem @ {}", captured_network_system);

    // --- Read ReflectionCtx from NetworkSystem ---
    // mReflectionCtx is a gsl::not_null<unique_ptr<cereal::ReflectionCtx>>
    // at a known offset in NetworkSystem. The offset must be determined from
    // the BDS binary (count member sizes or find via IDA).
    // For now, use getPacketReflectionCtx() via sigscan if available,
    // or hardcode the offset once known.

    // TODO: resolve mReflectionCtx offset or sigscan getPacketReflectionCtx
    // For now, placeholder:
    // auto *ctx = *reinterpret_cast<cereal::ReflectionCtx**>(
    //     static_cast<char*>(captured_network_system) + REFLECTION_CTX_OFFSET);

    // --- Enumerate packets ---

    auto *create_addr = findSig<sig::CREATE_PACKET>("MinecraftPackets::createPacket");
    if (!create_addr) {
        spdlog::error("Cannot proceed without createPacket");
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    auto createPacket = reinterpret_cast<CreatePacketFn>(create_addr);

    struct PacketInfo { int id; std::string name; };
    std::vector<PacketInfo> packets;

    for (int id = 0; ; id++) {
        if (id >= 200 && id <= 299) continue;
        try {
            auto pkt = createPacket(id);
            if (!pkt) break;
            packets.push_back({id, std::string(pkt->getName())});
        }
        catch (...) { break; }
    }

    spdlog::info("Found {} packets", packets.size());

    // --- Extract schemas via cereal reflection ---

    CerealSchemaReader reader;
    // TODO: pass actual ReflectionCtx* once offset is resolved
    // if (!reader.init(ctx)) { ... }

    spdlog::info("Generating .proto files...");
    // TODO: uncomment once reader.init works
    // reader.dumpRegisteredTypes(output_dir);
    // std::vector<ProtoGenerator::PacketEntry> entries;
    // for (const auto &pkt : packets) { ... }
    // ProtoGenerator gen;
    // gen.generate(entries, output_dir);

    spdlog::info("Done! Proto files written to: {}", output_dir.string());

    {
        std::ofstream done(output_dir / "DONE.txt");
        done << packets.size() << " packets enumerated\n";
    }

    spdlog::shutdown();
    FreeLibraryAndExitThread(static_cast<HMODULE>(param), 0);
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        CreateThread(nullptr, 0, DumpThread, hDll, 0, nullptr);
    }
    return TRUE;
}