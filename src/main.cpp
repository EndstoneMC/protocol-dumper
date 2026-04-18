#include <Windows.h>

#include <filesystem>
#include <print>
#include <string>

#include "cereal/Context.h"
#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"
#include "json_writer.h"
#include "visitor.h"

static void ListPackets()
{
    int count = 0;
    for (int id = 1; id < 1024; id++) {
        try {
            auto pk = MinecraftPackets::createPacket(static_cast<MinecraftPacketIds>(id));
            if (!pk) continue;
            ++count;
            if (pk->getSerializationMode() == SerializationMode::ManualOnly) {
                std::println(stderr, "[{}] {}: ManualOnly", static_cast<int>(pk->getId()), pk->getName());
            }
        }
        catch (...) {
            break;
        }
    }
    std::println("Enumerated {} packets", count);
}

static void DumpSchemas()
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    auto exe_dir = std::filesystem::path(exe_path).parent_path();
    auto output_dir = exe_dir / "data" / "protocol";
    std::filesystem::create_directories(output_dir);

    auto server = ServiceLocator<ServerInstance>::get();
    auto &network = static_cast<NetworkSystem &>(server->getNetwork());
    auto &ctx = network.getPacketReflectionCtx();

    cereal::DescriptionConfig config{};
    using Extra = cereal::DescriptionConfig::Extra;
    config.mContextArea = cereal::ContextArea::ALL;
    config.mExtraInfo = Extra::networkingExtraInfo | Extra::nonPublicFlag;
    config.mIsTopLevel = true;

    auto [types, packets] = proto::dumpProtocol(ctx, config);
    proto::output::write_json(packets, types, output_dir);
    std::println("Dumped {} types, {} packets to {}", types.size(), packets.size(), output_dir.string());
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
