#include <Windows.h>
// must be included after Windows.h
#include <TlHelp32.h>

#include <filesystem>
#include <string>
#include <thread>

#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

static DWORD findProcess(const std::wstring &name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    DWORD pid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return pid;
}

static bool inject(DWORD pid, const std::filesystem::path &dll_path)
{
    std::string path_str = dll_path.string();

    HANDLE proc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!proc) {
        spdlog::error("Failed to open process (error {})", GetLastError());
        return false;
    }

    void *remote_buf = VirtualAllocEx(proc, nullptr, path_str.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_buf) {
        spdlog::error("VirtualAllocEx failed (error {})", GetLastError());
        CloseHandle(proc);
        return false;
    }

    if (!WriteProcessMemory(proc, remote_buf, path_str.c_str(), path_str.size() + 1, nullptr)) {
        spdlog::error("WriteProcessMemory failed (error {})", GetLastError());
        VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    auto loadLibraryAddr =
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLibraryAddr, remote_buf, 0, nullptr);
    if (!thread) {
        spdlog::error("CreateRemoteThread failed (error {})", GetLastError());
        VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, INFINITE);

    CloseHandle(thread);
    VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
    CloseHandle(proc);
    return true;
}

int main(int argc, char *argv[])
{
    argparse::ArgumentParser program("injector", "1.0");
    program.add_description("Inject proto_dumper.dll into a running BDS process");

    program.add_argument("-p", "--process")
        .default_value(std::string("bedrock_server.exe"))
        .help("target process name");

    program.add_argument("-d", "--dll")
        .default_value(std::string(""))
        .help("path to DLL (default: proto_dumper.dll next to this exe)");

    program.add_argument("-t", "--timeout")
        .default_value(0)
        .scan<'i', int>()
        .help("seconds to wait for process (0 = wait forever)");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &e) {
        spdlog::error("{}", e.what());
        std::cerr << program;
        return 1;
    }

    auto process_name = program.get<std::string>("--process");
    std::wstring wprocess_name(process_name.begin(), process_name.end());

    std::filesystem::path dll_path;
    auto dll_arg = program.get<std::string>("--dll");
    if (dll_arg.empty()) {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        dll_path = std::filesystem::path(exe_path).parent_path() / "proto_dumper.dll";
    }
    else {
        dll_path = std::filesystem::absolute(dll_arg);
    }

    if (!std::filesystem::exists(dll_path)) {
        spdlog::error("DLL not found: {}", dll_path.string());
        return 1;
    }

    int timeout = program.get<int>("--timeout");
    spdlog::info("Waiting for {}...", process_name);

    DWORD pid = 0;
    auto start = std::chrono::steady_clock::now();
    while (!(pid = findProcess(wprocess_name))) {
        if (timeout > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= std::chrono::seconds(timeout)) {
                spdlog::error("Timed out waiting for {}", process_name);
                return 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    spdlog::info("Found {} (PID {})", process_name, pid);
    spdlog::info("Injecting {}...", dll_path.filename().string());

    if (!inject(pid, dll_path)) {
        spdlog::error("Injection failed");
        return 1;
    }

    spdlog::info("Injected. The DLL will dump schemas and unload itself.");
    return 0;
}
