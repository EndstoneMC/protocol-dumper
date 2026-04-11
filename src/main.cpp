#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <funchook.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "config.h"
#include "symbol.h"
#include "reader.h"
#include "generator.h"

// --- NetworkSystem::update hook ---

static NetworkSystem *captured_network_system = nullptr;
static HANDLE capture_event = nullptr;

// Original function pointer — funchook replaces this with the trampoline
static void (*orig_update)(NetworkSystem *, const std::vector<WeakEntityRef> *) = nullptr;

static void hooked_update(NetworkSystem *self, const std::vector<WeakEntityRef> *userList)
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

    if (!Config::instance().load(exe_dir / "config.json")) {
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    // --- Hook NetworkSystem::update to capture instance ---

    auto *update_addr = resolveSymbol(
        "?update@NetworkSystem@@QEAAXPEBVWeakEntityRef@@@Z");
    if (!update_addr) {
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    capture_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    orig_update = reinterpret_cast<decltype(orig_update)>(update_addr);

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

    spdlog::info("Captured NetworkSystem @ {}", static_cast<void *>(captured_network_system));

    // --- Get ReflectionCtx ---

    auto &ctx = captured_network_system->getPacketReflectionCtx();
    spdlog::info("ReflectionCtx @ {}", static_cast<const void *>(&ctx));

    // --- Enumerate packets ---

    struct PacketInfo { int id; std::string name; };
    std::vector<PacketInfo> packets;

    for (int id = 0; ; id++) {
        if (id >= 200 && id <= 299) continue;
        try {
            auto pkt = MinecraftPackets::createPacket(id);
            if (!pkt) break;
            packets.push_back({id, std::string(pkt->getName())});
        }
        catch (...) { break; }
    }

    spdlog::info("Found {} packets", packets.size());

    // --- Extract schemas ---

    CerealSchemaReader reader;
    if (!reader.init(const_cast<cereal::ReflectionCtx *>(&ctx))) {
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
