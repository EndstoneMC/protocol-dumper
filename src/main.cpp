#include <Windows.h>

#include <filesystem>
#include <print>
#include <string>
#include <vector>

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"
#include "generator.h"

static void DumpSchemas(HMODULE hDll)
{
    wchar_t dll_path[MAX_PATH];
    GetModuleFileNameW(hDll, dll_path, MAX_PATH);
    auto dll_dir = std::filesystem::path(dll_path).parent_path();

    auto output_dir = dll_dir / "out";
    std::filesystem::create_directories(output_dir);

    std::println("=== BDS Packet Schema Dump ===");

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
                std::println(stderr, "[{}] {} (ManualOnly)", static_cast<int>(pk->getId()), pk->getName());
            }
        }
        catch (...) {
            break;
        }
    }
    std::println("Found {} packets", packets.size());

    // --- Extract schemas ---
    auto server = ServiceLocator<ServerInstance>::get();
    auto &network = static_cast<NetworkSystem &>(server->getNetwork());
    auto &ctx = network.getPacketReflectionCtx();
    std::println("ServerInstance        @ {}", static_cast<const void *>(server.operator->()));
    std::println("NetworkSystem         @ {}", static_cast<const void *>(&network));
    std::println("NetworkSystem::vtable @ {}", *reinterpret_cast<void **>(&network));
    std::println("ReflectionCtx         @ {}", static_cast<const void *>(&ctx));

    std::vector<SchemaGenerator::PacketEntry> entries;
    auto &meta_ctx = ctx.internal().mMetaCtx;

    cereal::DescriptionConfig config{};
    config.mContextArea = cereal::ContextArea::ALL;
    config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
    config.mIsTopLevel = true;

    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        cereal::internal::BasicSchema::TypeDescriptor *descriptor = meta_type.custom();
        if (!descriptor) {
            continue;
        }

        auto it = descriptor->mUserPropertiesMap.find("[cereal:packet]");
        if (it == descriptor->mUserPropertiesMap.end()) {
            continue;
        }

        auto packet_name = std::string(meta_type.info().name());
        if (const auto pos = packet_name.find(' '); pos != std::string::npos) {
            packet_name.erase(0, pos + 1);
        }

        int packet_id = entt::any_cast<int>(it->second.second);

        // Follow readAndWriteAs: find the meta function whose return type
        // has a TypeDescriptor with a BasicSchema — that's the payload type.
        cereal::internal::BasicSchema *payload_schema = nullptr;
        for (auto &&[func_id, func] : meta_type.func()) {
            auto ret = func.ret();
            auto *ret_desc = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(ret.custom());
            if (ret_desc && ret_desc->mPtr) {
                payload_schema = ret_desc->mPtr.get();
                break;
            }
        }

        if (!payload_schema) {
            std::println(stderr, "[{}] {} - no payload schema", packet_id, packet_name);
            continue;
        }

        auto desc = payload_schema->description(ctx.internal(), config);

        std::println("[{}] {}", packet_id, packet_name);
        entries.push_back({packet_id, std::move(packet_name), desc});
    }

    std::println("{} / {} packets have cereal schemas", entries.size(), packets.size());

    SchemaGenerator gen;
    gen.generate(entries, output_dir);
    std::println("Done! Output: {}", output_dir.string());
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        DumpSchemas(hDll);
    }
    return FALSE;
}
