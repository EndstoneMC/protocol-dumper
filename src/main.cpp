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

static void logToFile(const std::filesystem::path &dir, const std::string &msg)
{
    std::ofstream log(dir / "dump_log.txt", std::ios::app);
    log << msg << "\n";
}

static void dumpSchemas(HMODULE hDll)
{
    std::filesystem::path output_dir = "D:\\bds_packet_schemas";
    std::filesystem::create_directories(output_dir);

    logToFile(output_dir, "=== BDS Packet Schema Dump ===");

    auto base = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
    auto createPacket = reinterpret_cast<CreatePacketFn>(base + CREATE_PACKET_RVA);

    logToFile(output_dir, "Base address: 0x" + std::to_string(base));

    // --- Enumerate packets by ID ---
    // Start from 0, skip 200-299 (title-specific), stop when factory returns nullptr.
    struct PacketInfo {
        int id;
        std::string name;
    };
    std::vector<PacketInfo> packets;

    for (int id = 0; ; id++) {
        if (id >= 200 && id <= 299) continue;

        try {
            auto pkt = createPacket(id);
            if (!pkt) break;  // end of valid IDs

            packets.push_back({id, std::string(pkt->getName())});
        }
        catch (...) {
            break;
        }
    }

    logToFile(output_dir, "Found " + std::to_string(packets.size()) + " packets");

    // --- Extract schemas via cereal reflection ---
    CerealSchemaReader reader;
    if (!reader.init(base, [&](const std::string &msg) {
            logToFile(output_dir, msg);
        })) {
        logToFile(output_dir, "FATAL: cereal init failed");
        FreeLibraryAndExitThread(hDll, 1);
        return;
    }

    reader.dumpRegisteredTypes(output_dir);

    // Match each packet to its cereal schema by payload type name
    std::vector<ProtoGenerator::PacketEntry> entries;

    for (const auto &pkt : packets) {
        // Convention: payload type = name + "PacketPayload" or name + "Payload"
        auto desc = reader.getSchema(pkt.name + "PacketPayload");
        if (!desc) desc = reader.getSchema(pkt.name + "Payload");

        if (desc) {
            entries.push_back({pkt.id, pkt.name, std::move(*desc)});
            logToFile(output_dir, "  OK [" + std::to_string(pkt.id) + "] " + pkt.name);
        }
        else {
            logToFile(output_dir, "  SKIP [" + std::to_string(pkt.id) + "] " + pkt.name +
                                      " (no cereal schema found)");
        }
    }

    logToFile(output_dir, "\n" + std::to_string(entries.size()) + " / " +
                              std::to_string(packets.size()) + " packets have cereal schemas");

    // --- Generate .proto files ---
    logToFile(output_dir, "\nGenerating .proto files...");

    ProtoGenerator gen;
    gen.generate(entries, output_dir);

    logToFile(output_dir, "Done! Proto files written to: " + output_dir.string());

    {
        std::ofstream done(output_dir / "DONE.txt");
        done << entries.size() << " / " << packets.size() << " packets dumped\n";
    }

    FreeLibraryAndExitThread(hDll, 0);
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        dumpSchemas(hDll);
    }
    return TRUE;
}
