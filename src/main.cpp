#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "common/Packet.h"
#include "reader.h"
#include "generator.h"

// RVA for MinecraftPackets::createPacket — from Endstone's bedrock_symbols.generated.h
// This is version-specific. Update when BDS updates.
constexpr uintptr_t CREATE_PACKET_RVA = 19037872;

DWORD WINAPI DumpThread(LPVOID param)
{
    // Resolve output dir relative to the DLL location
    wchar_t dll_path[MAX_PATH];
    GetModuleFileNameW(static_cast<HMODULE>(param), dll_path, MAX_PATH);
    std::filesystem::path output_dir = std::filesystem::path(dll_path).parent_path() / "data" / "proto";
    std::filesystem::create_directories(output_dir);

    auto logger = spdlog::basic_logger_mt("dumper", (output_dir / "dump_log.txt").string(), true);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("=== BDS Packet Schema Dump ===");

    auto base = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
    auto createPacket = reinterpret_cast<CreatePacketFn>(base + CREATE_PACKET_RVA);

    // Enumerate packets by ID: start from 0, skip 200-299, stop on nullptr.
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

    // Extract schemas via cereal reflection
    CerealSchemaReader reader;
    if (!reader.init(base)) {
        spdlog::error("cereal init failed");
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

    spdlog::info("Generating .proto files...");
    ProtoGenerator gen;
    gen.generate(entries, output_dir);
    spdlog::info("Done! Proto files written to: {}", output_dir.string());

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
