#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "common/Packet.h"
#include "reader.h"
#include "generator.h"

// RVA for MinecraftPackets::createPacket — from Endstone's bedrock_symbols.generated.h
// This is version-specific. Update when BDS updates.
constexpr uintptr_t CREATE_PACKET_RVA = 19037872;

DWORD WINAPI DumpThread(LPVOID param)
{
    std::filesystem::path output_dir = "D:\\bds_packet_schemas";
    std::filesystem::create_directories(output_dir);

    std::ofstream log(output_dir / "dump_log.txt", std::ios::trunc);
    auto logMsg = [&](const std::string &msg) { log << msg << "\n"; log.flush(); };

    logMsg("=== BDS Packet Schema Dump ===");

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

    logMsg("Found " + std::to_string(packets.size()) + " packets");

    // Extract schemas via cereal reflection
    CerealSchemaReader reader;
    if (!reader.init(base, logMsg)) {
        logMsg("FATAL: cereal init failed");
        FreeLibraryAndExitThread(static_cast<HMODULE>(param), 1);
    }

    reader.dumpRegisteredTypes(output_dir);

    std::vector<ProtoGenerator::PacketEntry> entries;
    for (const auto &pkt : packets) {
        auto desc = reader.getSchema(pkt.name + "PacketPayload");
        if (!desc) desc = reader.getSchema(pkt.name + "Payload");

        if (desc) {
            entries.push_back({pkt.id, pkt.name, std::move(*desc)});
            logMsg("  OK [" + std::to_string(pkt.id) + "] " + pkt.name);
        }
        else {
            logMsg("  SKIP [" + std::to_string(pkt.id) + "] " + pkt.name);
        }
    }

    logMsg(std::to_string(entries.size()) + " / " +
           std::to_string(packets.size()) + " packets have cereal schemas");

    logMsg("Generating .proto files...");
    ProtoGenerator gen;
    gen.generate(entries, output_dir);
    logMsg("Done! Proto files written to: " + output_dir.string());

    {
        std::ofstream done(output_dir / "DONE.txt");
        done << entries.size() << " / " << packets.size() << " packets dumped\n";
    }

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
