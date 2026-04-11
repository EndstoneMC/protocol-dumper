#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "cereal/Context.h"
#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"

DWORD WINAPI DumpThread(LPVOID param)
{
    // Resolve paths: config lives next to the DLL, output next to the executable
    wchar_t dll_path[MAX_PATH];
    GetModuleFileNameW(static_cast<HMODULE>(param), dll_path, MAX_PATH);
    auto dll_dir = std::filesystem::path(dll_path).parent_path();

    auto output_dir = dll_dir / "out";
    std::filesystem::create_directories(output_dir);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((output_dir / "dump_log.txt").string(), true);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("dumper", spdlog::sinks_init_list{console_sink, file_sink});
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("=== BDS Packet Schema Dump ===");

    // --- Hook NetworkSystem::update to capture instance ---
    //
    // auto *update_addr = getSymbol(
    //     "NetworkSystem::update");
    // if (!update_addr) {
    //     FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    // }
    //
    // capture_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    // orig_update = reinterpret_cast<decltype(orig_update)>(update_addr);
    //
    // funchook_t *hook = funchook_create();
    // if (funchook_prepare(hook, reinterpret_cast<void **>(&orig_update),
    //                      reinterpret_cast<void *>(hooked_update)) != 0 ||
    //     funchook_install(hook, 0) != 0) {
    //     spdlog::error("Hook failed: {}", funchook_error_message(hook));
    //     FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    // }
    //
    // spdlog::info("Waiting for NetworkSystem::update...");
    // WaitForSingleObject(capture_event, INFINITE);
    // CloseHandle(capture_event);
    // funchook_uninstall(hook, 0);
    // funchook_destroy(hook);
    //
    // spdlog::info("Captured NetworkSystem @ {}", static_cast<void *>(captured_network_system));
    //
    // // --- Get ReflectionCtx via sigscanned member offset ---
    //
    // auto *offset_addr = getSymbol("NetworkSystem::mReflectionCtx");
    // if (!offset_addr) {
    //     FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    // }
    // auto ctx_offset = *reinterpret_cast<int32_t *>(offset_addr);
    // auto *ctx = *reinterpret_cast<cereal::ReflectionCtx **>(
    //     reinterpret_cast<char *>(captured_network_system) + ctx_offset);
    // spdlog::info("mReflectionCtx offset=0x{:X}, ctx @ {}", ctx_offset, static_cast<void *>(ctx));

    // --- Enumerate packets ---
    std::vector<std::shared_ptr<Packet>> packets;
    for (int id = 1; id < 500; id++) {
        try {
            auto pk = MinecraftPackets::createPacket(id);
            if (!pk) {
                continue;
            }
            packets.push_back(pk);
        }
        catch (...) {
            break;
        }
    }
    spdlog::info("Found {} packets", packets.size());

    // --- Extract schemas ---

    // CerealSchemaReader reader;
    // if (!reader.init(ctx)) {
    //     FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    // }
    //
    // reader.dumpRegisteredTypes(output_dir);
    //
    // std::vector<ProtoGenerator::PacketEntry> entries;
    // for (const auto &pkt : packets) {
    //     auto desc = reader.getSchema(pkt.name + "PacketPayload");
    //     if (!desc) desc = reader.getSchema(pkt.name + "Payload");
    //
    //     if (desc) {
    //         entries.push_back({pkt.id, pkt.name, std::move(*desc)});
    //         spdlog::info("  OK [{}] {}", pkt.id, pkt.name);
    //     }
    //     else {
    //         spdlog::warn("  SKIP [{}] {}", pkt.id, pkt.name);
    //     }
    // }
    //
    // spdlog::info("{} / {} packets have cereal schemas", entries.size(), packets.size());
    //
    // ProtoGenerator gen;
    // gen.generate(entries, output_dir);
    // spdlog::info("Done! Output: {}", output_dir.string());
    //
    // {
    //     std::ofstream done(output_dir / "DONE.txt");
    //     done << entries.size() << " / " << packets.size() << " packets dumped\n";
    // }

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
