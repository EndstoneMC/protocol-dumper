#include <Windows.h>

#include <filesystem>
#include <map>
#include <print>
#include <string>
#include <vector>

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"
#include "generator.h"

static void fixNames(cereal::SchemaDescription &desc, const std::map<std::string, std::string> &names)
{
    if (desc.mName) {
        if (auto it = names.find(*desc.mName); it != names.end()) {
            desc.mName = it->second;
        }
    }
    if (desc.mMembers) {
        for (auto &[k, member] : *desc.mMembers) {
            fixNames(member, names);
        }
    }
    if (desc.mValueType) {
        fixNames(*desc.mValueType, names);
    }
    if (desc.mKeyType) {
        fixNames(*desc.mKeyType, names);
    }
    if (desc.mMappedType) {
        fixNames(*desc.mMappedType, names);
    }
    if (desc.mParents) {
        for (auto &p : *desc.mParents) {
            fixNames(p, names);
        }
    }
    if (desc.mSetters) {
        for (auto &s : *desc.mSetters) {
            fixNames(s, names);
        }
    }
}

static void DumpSchemas()
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    auto exe_dir = std::filesystem::path(exe_path).parent_path();

    auto output_dir = exe_dir / "data" / "protocol";
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

    std::vector<SchemaGenerator::PacketEntry> packet_entries;
    std::vector<SchemaGenerator::TypeEntry> type_entries;
    auto &meta_ctx = ctx.internal().mMetaCtx;

    // Build name map: BDS mName (spaces) → entt proper name (::)
    std::map<std::string, std::string> name_map;
    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto *descriptor = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(meta_type.custom());
        if (!descriptor) {
            continue;
        }

        auto proper = std::string(meta_type.info().name());
        if (auto sp = proper.find(' '); sp != std::string::npos) {
            proper.erase(0, sp + 1);
        }

        if (!descriptor->mName.empty() && descriptor->mName != proper) {
            name_map[descriptor->mName] = proper;
        }
    }

    cereal::DescriptionConfig config{};
    config.mContextArea = cereal::ContextArea::ALL;
    config.mExtraInfo = cereal::DescriptionConfig::Extra::networkingExtraInfo;
    config.mIsTopLevel = true;

    for (auto &&[id, meta_type] : entt::resolve(meta_ctx)) {
        auto *descriptor = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(meta_type.custom());
        if (!descriptor || !descriptor->mPtr) {
            continue;
        }

        auto name = std::string(meta_type.info().name());
        if (auto sp = name.find(' '); sp != std::string::npos) {
            name.erase(0, sp + 1);
        }

        auto it = descriptor->mUserPropertiesMap.find("[cereal:packet]");
        if (it != descriptor->mUserPropertiesMap.end()) {
            int packet_id = entt::any_cast<int>(it->second.second);

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
                std::println(stderr, "[{}] {} - no payload schema", packet_id, name);
                continue;
            }

            auto desc = payload_schema->description(ctx.internal(), config);
            fixNames(desc, name_map);
            std::println("[{}] {}", packet_id, name);
            packet_entries.push_back({packet_id, std::move(name), std::move(desc)});
        }
        else {
            if (name.ends_with("Payload")) {
                continue;
            }

            auto desc = descriptor->mPtr->description(ctx.internal(), config);
            fixNames(desc, name_map);
            type_entries.push_back({std::move(name), std::move(desc)});
        }
    }

    std::println("{} / {} packets have cereal schemas", packet_entries.size(), packets.size());
    std::println("{} types", type_entries.size());

    SchemaGenerator gen;
    gen.generate(packet_entries, type_entries, output_dir);
    std::println("Done! Output: {}", output_dir.string());
}

static void signalDone()
{
    if (auto hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "proto_dumper_done")) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        try {
            DumpSchemas();
        }
        catch (const std::exception &e) {
            std::println(stderr, "FATAL: {}", e.what());
        }
        signalDone();
        return FALSE;
    }
    return TRUE;
}
