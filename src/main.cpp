#include <Windows.h>

#include <filesystem>
#include <print>
#include <string>

#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"
#include "json_writer.h"
#include "visitor.h"

int main()
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    auto exe_dir = std::filesystem::path(exe_path).parent_path();
    auto output_dir = exe_dir / "data" / "protocol";
    std::filesystem::create_directories(output_dir);

    auto server = ServiceLocator<ServerInstance>::get();
    auto &network = static_cast<NetworkSystem &>(server->getNetwork());
    auto &ctx = network.getPacketReflectionCtx();

    proto::Visitor visitor{ctx};
    auto protocol = visitor.dump();

    proto::JsonWriter writer(output_dir);
    writer.write(protocol);

    std::println("Dumped {} types, {} packets to {}", protocol.types.size(), protocol.packets.size(),
                 output_dir.string());
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        try {
            main();
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
