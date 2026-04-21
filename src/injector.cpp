#include <Windows.h>
// must be included after Windows.h
#include <TlHelp32.h>

#include <filesystem>
#include <string>
#include <thread>

#include <print>

#include <argparse/argparse.hpp>

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
        std::println("Failed to open process (error {})", GetLastError());
        return false;
    }

    void *remote_buf = VirtualAllocEx(proc, nullptr, path_str.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_buf) {
        std::println("VirtualAllocEx failed (error {})", GetLastError());
        CloseHandle(proc);
        return false;
    }

    if (!WriteProcessMemory(proc, remote_buf, path_str.c_str(), path_str.size() + 1, nullptr)) {
        std::println("WriteProcessMemory failed (error {})", GetLastError());
        VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    auto loadLibraryAddr =
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLibraryAddr, remote_buf, 0, nullptr);
    if (!thread) {
        std::println("CreateRemoteThread failed (error {})", GetLastError());
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
    program.add_description("Inject protocol_dumper.dll into a running BDS process");

    program.add_argument("-p", "--process")
        .default_value(std::string("bedrock_server.exe"))
        .help("target process name");

    program.add_argument("-d", "--dll")
        .default_value(std::string(""))
        .help("path to DLL (default: protocol_dumper.dll next to this exe)");

    program.add_argument("-t", "--timeout")
        .default_value(0)
        .scan<'i', int>()
        .help("seconds to wait for process (0 = wait forever)");

    program.add_argument("--dll-timeout")
        .default_value(5)
        .scan<'i', int>()
        .help("seconds to wait for the DLL to finish (0 = wait forever)");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &e) {
        std::println("{}", e.what());
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
        dll_path = std::filesystem::path(exe_path).parent_path() / "protocol_dumper.dll";
    }
    else {
        dll_path = std::filesystem::absolute(dll_arg);
    }

    if (!std::filesystem::exists(dll_path)) {
        std::println("DLL not found: {}", dll_path.string());
        return 1;
    }

    int timeout = program.get<int>("--timeout");
    std::println("Waiting for {}...", process_name);

    DWORD pid = 0;
    auto start = std::chrono::steady_clock::now();
    while (!(pid = findProcess(wprocess_name))) {
        if (timeout > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= std::chrono::seconds(timeout)) {
                std::println("Timed out waiting for {}", process_name);
                return 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::println("Found {} (PID {})", process_name, pid);

    HANDLE hEvent = CreateEventA(nullptr, TRUE, FALSE, "protocol_dumper_done");

    std::println("Injecting {}...", dll_path.filename().string());

    if (!inject(pid, dll_path)) {
        std::println("Injection failed");
        if (hEvent) CloseHandle(hEvent);
        return 1;
    }

    int dll_timeout = program.get<int>("--dll-timeout");
    std::println("Waiting for DLL to finish...");
    if (hEvent) {
        DWORD wait_ms = dll_timeout > 0 ? static_cast<DWORD>(dll_timeout) * 1000 : INFINITE;
        if (WaitForSingleObject(hEvent, wait_ms) == WAIT_TIMEOUT) {
            std::println("Timed out waiting for DLL after {}s", dll_timeout);
        }
        CloseHandle(hEvent);
    }

    std::println("Done.");
    return 0;
}
