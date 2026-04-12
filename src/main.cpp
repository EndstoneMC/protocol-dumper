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
#include "generator.h"
#include "reader.h"

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
            if (pk->getSerializationMode() == SerializationMode::ManualOnly) {
                spdlog::warn("[{}] {}", static_cast<int>(pk->getId()), pk->getName());
            }
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
    spdlog::info("ServerInstance        @ {}", fmt::ptr(server.operator->()));
    spdlog::info("NetworkSystem         @ {}", fmt::ptr(&network));
    spdlog::info("NetworkSystem::vtable @ {}", fmt::ptr(*reinterpret_cast<void **>(&network)));
    spdlog::info("ReflectionCtx         @ {}", fmt::ptr(&ctx));

    std::vector<SchemaGenerator::PacketEntry> entries;
    auto &meta_ctx = ctx.internal().mMetaCtx;
    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto &info = meta_type.info();
        if (!info.name().ends_with("PacketPayload")) {
            continue;
        }
        spdlog::info("{}", info.name());
        try {
            auto &schema = cereal::internal::BasicSchema::lookup(meta_ctx, info);
            spdlog::info("Schema: {}", fmt::ptr(&schema));
            spdlog::info("isGreedy: {}", static_cast<int>(schema.isGreedy(meta_ctx)));
            spdlog::info("minVariantPriorityLevel: {}", static_cast<int>(schema.minVariantPriorityLevel(meta_ctx)));
            cereal::DescriptionConfig config{};
            config.mContextArea = cereal::ContextArea::ALL;
            config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
            config.mIsTopLevel = true;
            auto desc = schema.description(ctx.internal(), config);
            spdlog::info("Description: {}", fmt::ptr(&desc));
            auto raw_name = std::string(info.name());
            auto sp = raw_name.find(' ');
            if (sp != std::string::npos) raw_name.erase(0, sp + 1);
            if (raw_name.ends_with("PacketPayload")) raw_name.erase(raw_name.size() - 13);
            entries.push_back({0, std::move(raw_name), desc});
        }
        catch (std::exception &e) {
            spdlog::error(e.what());
        }
    }

    // CerealSchemaReader reader(ctx);
    // reader.dumpRegisteredTypes(output_dir);
    //
    // std::vector<ProtoGenerator::PacketEntry> entries;
    // for (const auto &pkt : packets) {
    //     auto name = std::string(pkt->getName());
    //     auto desc = reader.getSchema(name + "PacketPayload");
    //     if (!desc) {
    //         desc = reader.getSchema(name + "Payload");
    //     }
    //
    //     if (desc) {
    //         // entries.push_back({pkt->getName(),name, std::move(*desc)});
    //         spdlog::info("  OK [{}] {}", static_cast<int>(pkt->getId()), pkt->getName());
    //     }
    //     else {
    //         spdlog::warn("  SKIP [{}] {}", static_cast<int>(pkt->getId()), pkt->getName());
    //     }
    // }
    spdlog::info("{} / {} packets have cereal schemas", entries.size(), packets.size());
    //
    SchemaGenerator gen;
    gen.generate(entries, output_dir);
    spdlog::info("Done! Output: {}", output_dir.string());
    //
    // {
    //     std::ofstream done(output_dir / "DONE.txt");
    //     done << entries.size() << " / " << packets.size() << " packets dumped\n";
    // }

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
