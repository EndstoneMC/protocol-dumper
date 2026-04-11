#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <funchook.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "common/Packet.h"
#include "config.h"
#include "reader.h"
#include "generator.h"

static Config g_config;

// --- NetworkSystem::update hook ---

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

DWORD WINAPI DumpThread(LPVOID param)
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    auto exe_dir = std::filesystem::path(exe_path).parent_path();
    auto output_dir = exe_dir / "data" / "proto";
    std::filesystem::create_directories(output_dir);

    auto logger = spdlog::basic_logger_mt("dumper", (output_dir / "dump_log.txt").string(), true);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("=== BDS Packet Schema Dump ===");

    if (!g_config.load(exe_dir / "config.json")) {
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    // --- Hook NetworkSystem::update to capture instance ---

    auto *update_addr = g_config.resolve("NetworkSystem::update");
    if (!update_addr) {
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    capture_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    orig_update = reinterpret_cast<NetworkSystemUpdateFn>(update_addr);

    funchook_t *hook = funchook_create();
    if (funchook_prepare(hook, reinterpret_cast<void **>(&orig_update),
                         reinterpret_cast<void *>(hooked_update)) != 0 ||
        funchook_install(hook, 0) != 0) {
        spdlog::error("Hook failed: {}", funchook_error_message(hook));
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    spdlog::info("Waiting for NetworkSystem::update...");
    WaitForSingleObject(capture_event, INFINITE);
    CloseHandle(capture_event);
    funchook_uninstall(hook, 0);
    funchook_destroy(hook);

    spdlog::info("Captured NetworkSystem @ {}", captured_network_system);

    // --- Read ReflectionCtx from NetworkSystem ---

    auto ctx_offset = g_config.findOffset("NetworkSystem::mReflectionCtx");
    if (ctx_offset == 0) {
        spdlog::error("NetworkSystem::mReflectionCtx offset not configured");
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    auto *ctx = *reinterpret_cast<cereal::ReflectionCtx **>(
        static_cast<char *>(captured_network_system) + ctx_offset);
    spdlog::info("ReflectionCtx @ {}", static_cast<void *>(ctx));

    // --- Enumerate packets ---

    auto *create_addr = g_config.resolve("MinecraftPackets::createPacket");
    if (!create_addr) {
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

    // --- Extract schemas ---

    CerealSchemaReader reader;
    if (!reader.init(ctx, g_config)) {
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    reader.dumpRegisteredTypes(output_dir);

    std::vector<ProtoGenerator::PacketEntry> entries;
    for (const auto &pkt : packets) {
        auto desc = reader.getSchema(pkt.name + "PacketPayload");
        if (!desc) desc = reader.getSchema(pkt.name + "Payload");

        if (desc) {
            entries.push_back({pkt.id, pkt.name, std::move(*desc)});
            spdlog::info("  OK [{}] {}", pkt.id, pkt.name);
        }
        else {
            spdlog::warn("  SKIP [{}] {}", pkt.id, pkt.name);
        }
    }

    spdlog::info("{} / {} packets have cereal schemas", entries.size(), packets.size());

    ProtoGenerator gen;
    gen.generate(entries, output_dir);
    spdlog::info("Done! Output: {}", output_dir.string());

    {
        std::ofstream done(output_dir / "DONE.txt");
        done << entries.size() << " / " << packets.size() << " packets dumped\n";
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
