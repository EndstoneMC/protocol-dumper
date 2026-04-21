#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <print>

#include "common/NetworkSystem.h"
#include "common/Packet.h"
#include "common/ServiceLocator.h"
#include "visitor.h"

template <class... Ts>
struct overloads : Ts... {
    using Ts::operator()...;
};

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

    proto::Visitor visitor(ctx);
    for (const auto &type : visitor.getTypes() | std::views::values) {
        std::visit(overloads{[&](auto &&arg) {
                       using T = std::decay_t<decltype(arg)>;
                       auto path = output_dir;
                       if constexpr (std::is_same_v<T, proto::Packet>) {
                           path /= "packets";
                       }
                       else {
                           path /= "types";
                       }
                       create_directories(path);
                       auto filename = arg.name;
                       std::ranges::replace(filename, ':', '_');
                       std::ofstream f(path / (filename + ".json"));
                       f << nlohmann::ordered_json(arg).dump(4);
                   }},
                   type);
    }
    std::println("Dumped to {}", output_dir.string());
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
            std::println(stderr, "!!! FATAL: {}", e.what());
        }
        if (auto hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "protocol_dumper_done")) {
            SetEvent(hEvent);
            CloseHandle(hEvent);
        }
        return FALSE;
    }
    return TRUE;
}
