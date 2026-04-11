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

    // --- Enumerate packets ---
    std::vector<std::shared_ptr<Packet>> packets;
    for (int id = 1; id < 500; id++) {
        try {
            auto pk = MinecraftPackets::createPacket(static_cast<MinecraftPacketIds>(id));
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
    auto server = ServiceLocator<ServerInstance>::get();
    auto &network = static_cast<NetworkSystem &>(server->getNetwork());
    auto &ctx = network.getPacketReflectionCtx();
    spdlog::info("ServerInstance: {}", fmt::ptr(server.operator->()));
    spdlog::info("NetworkSystem: {}", fmt::ptr(&network));
    spdlog::info("NetworkSystem::vtable: {}", fmt::ptr(*reinterpret_cast<void **>(&network)));
    spdlog::info("ReflectionCtx: {}", fmt::ptr(&ctx));

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
