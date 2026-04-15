#include <Windows.h>

#include <filesystem>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include "cereal/Context.h"
#include "cereal/schema/BasicSchema.h"
#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"
#include "json_writer.h"
#include "visitor.h"

// Handles combined forms such as "enum class Foo::Bar" by stripping repeatedly.
static std::string_view stripTypePrefix(std::string_view name)
{
    constexpr std::string_view kPrefixes[] = {"struct ", "class ", "enum ", "union "};
    for (bool changed = true; changed;) {
        changed = false;
        for (auto p : kPrefixes) {
            if (name.starts_with(p)) {
                name.remove_prefix(p.size());
                changed = true;
                break;
            }
        }
    }
    return name;
}

static std::string qualifiedName(const entt::meta_type &t)
{
    return std::string{stripTypePrefix(t.info().name())};
}

static cereal::internal::BasicSchema *getSchema(const entt::meta_type &t)
{
    auto *d = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(t.custom());
    return (d && d->mPtr) ? d->mPtr.get() : nullptr;
}

static void ListPackets()
{
    int i = 0;
    std::println("=== BDS Packet List ===");
    for (int id = 1; id < 1024; id++) {
        try {
            auto pk = MinecraftPackets::createPacket(static_cast<MinecraftPacketIds>(id));
            if (!pk) {
                continue;
            }
            ++i;
            if (pk->getSerializationMode() == SerializationMode::ManualOnly) {
                std::println(stderr, "[{}] {} has manual only serialization mode.", static_cast<int>(pk->getId()),
                             pk->getName());
            }
        }
        catch (...) {
            break;
        }
    }
    std::println("Found {} packets", i);
}

static void DumpSchemas()
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    auto exe_dir = std::filesystem::path(exe_path).parent_path();

    auto output_dir = exe_dir / "data" / "protocol";
    std::filesystem::create_directories(output_dir);

    std::println("=== BDS Packet Schema Dump ===");

    std::vector<std::shared_ptr<Packet>> packets;
    for (int id = 1; id < 500; id++) {
        try {
            auto pk = MinecraftPackets::createPacket(static_cast<MinecraftPacketIds>(id));
            if (!pk) {
                continue;
            }
            packets.push_back(pk);
            if (pk->getSerializationMode() == SerializationMode::ManualOnly) {
                std::println(stderr, "[{}] {} has manual only serialization mode.", static_cast<int>(pk->getId()),
                             pk->getName());
            }
        }
        catch (...) {
            break;
        }
    }
    std::println("Found {} packets", packets.size());

    auto server = ServiceLocator<ServerInstance>::get();
    auto &network = static_cast<NetworkSystem &>(server->getNetwork());
    auto &ctx = network.getPacketReflectionCtx();
    std::println("ServerInstance        @ {}", static_cast<const void *>(server.operator->()));
    std::println("NetworkSystem         @ {}", static_cast<const void *>(&network));
    std::println("NetworkSystem::vtable @ {}", *reinterpret_cast<void **>(&network));
    std::println("ReflectionCtx         @ {}", static_cast<const void *>(&ctx));

    auto &meta_ctx = ctx.internal().mMetaCtx;

    cereal::DescriptionConfig config{};
    using Extra = cereal::DescriptionConfig::Extra;
    config.mContextArea = cereal::ContextArea::ALL;
    config.mExtraInfo = Extra::networkingExtraInfo | Extra::nonPublicFlag;
    config.mIsTopLevel = true;

    std::println("Resolving meta context...");

    struct PacketSource {
        int id;
        std::string name;
        cereal::SchemaDescription desc;
    };
    std::vector<PacketSource> packet_sources;
    std::unordered_map<std::string, cereal::SchemaDescription> type_sources;

    int i = 0;
    for (auto &&meta_type : entt::resolve(meta_ctx) | std::views::values) {
        std::println("{}: {}", ++i, meta_type.info().name());

        auto *descriptor = static_cast<cereal::internal::BasicSchema::TypeDescriptor *>(meta_type.custom());
        if (!descriptor || !descriptor->mPtr) {
            continue;
        }

        auto name = qualifiedName(meta_type);
        descriptor->mName = name;

        if (descriptor->mUserPropertiesMap.contains("[cereal:packet]")) {
            int packet_id = entt::any_cast<int>(descriptor->mUserPropertiesMap["[cereal:packet]"].second);
            std::println("[{}] {}", packet_id, name);

            cereal::internal::BasicSchema *payload_schema = nullptr;
            for (auto &&[func_id, func] : meta_type.func()) {
                if (func.arity() != 0) continue;
                auto return_type = func.ret();
                if (qualifiedName(return_type) != name + "Payload") continue;
                payload_schema = getSchema(return_type);
                if (payload_schema) break;
            }
            if (!payload_schema) {
                std::println(stderr, "ERROR - [{}] {}: failed to find payload schema", packet_id, name);
                continue;
            }
            packet_sources.push_back({packet_id, name, payload_schema->description(ctx.internal(), config)});
        }
        else {
            type_sources.emplace(name, descriptor->mPtr->description(ctx.internal(), config));
        }
    }

    // Alias pre-pass: detect readAndWriteAs redirects. A cereal type T is an alias
    // for U iff T's meta-type has a 0-arity func whose return type is a different
    // named cereal type. Packets are gated on [cereal:packet] and are never aliases.
    proto::Visitor::AliasMap aliases;
    for (auto &&meta_type : entt::resolve(meta_ctx) | std::views::values) {
        auto from = qualifiedName(meta_type);
        if (!type_sources.contains(from)) continue;
        for (auto &&[func_id, func] : meta_type.func()) {
            if (func.arity() != 0) continue;
            auto to = qualifiedName(func.ret());
            if (to == from) continue;
            if (auto it = type_sources.find(to); it != type_sources.end()) {
                aliases.emplace(from, &it->second);
                break;
            }
        }
    }
    // Collapse alias chains: a -> b -> c becomes a -> c.
    for (auto &[from, target] : aliases) {
        std::unordered_map<std::string, bool> seen;
        while (target) {
            auto target_name = target->mName.value_or("");
            if (!seen.emplace(target_name, true).second) {
                break;  // cycle guard
            }
            auto next = aliases.find(target_name);
            if (next == aliases.end() || next->second == target) {
                break;
            }
            target = next->second;
        }
    }

    proto::Visitor visitor{meta_ctx, std::move(aliases)};

    for (const auto &[name, desc] : type_sources) {
        visitor.visitClass(name, desc);
    }
    for (const auto &src : packet_sources) {
        visitor.visitPacket(src.id, src.name, src.desc);
    }

    proto::output::JsonWriter writer;
    writer.write(visitor.packets(), visitor.classes(), output_dir);
    std::println("Done! Output: {}", output_dir.string());
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        try {
            ListPackets();
            DumpSchemas();
        }
        catch (const std::exception &e) {
            std::println(stderr, "FATAL: {}", e.what());
        }
        if (auto hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "proto_dumper_done")) {
            SetEvent(hEvent);
            CloseHandle(hEvent);
        }
        return FALSE;
    }
    return TRUE;
}
