#include <Windows.h>
#include <TlHelp32.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

static DWORD findProcess(const wchar_t *name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    DWORD pid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
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
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!proc) {
        std::cerr << "Failed to open process (error " << GetLastError() << ")\n";
        return false;
    }

    void *remote_buf = VirtualAllocEx(proc, nullptr, path_str.size() + 1,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_buf) {
        std::cerr << "VirtualAllocEx failed (error " << GetLastError() << ")\n";
        CloseHandle(proc);
        return false;
    }

    if (!WriteProcessMemory(proc, remote_buf, path_str.c_str(), path_str.size() + 1, nullptr)) {
        std::cerr << "WriteProcessMemory failed (error " << GetLastError() << ")\n";
        VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    auto loadLibraryAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLibraryAddr, remote_buf, 0, nullptr);
    if (!thread) {
        std::cerr << "CreateRemoteThread failed (error " << GetLastError() << ")\n";
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

int main()
{
    // Resolve DLL path relative to this exe
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::filesystem::path dll_path = std::filesystem::path(exe_path).parent_path() / "proto_dumper.dll";

    if (!std::filesystem::exists(dll_path)) {
        std::cerr << "DLL not found: " << dll_path.string() << "\n";
        return 1;
    }

    std::cout << "Waiting for bedrock_server.exe...\n";

    DWORD pid = 0;
    while (!(pid = findProcess(L"bedrock_server.exe"))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "Found bedrock_server.exe (PID " << pid << ")\n";
    std::cout << "Injecting " << dll_path.filename().string() << "...\n";

    if (!inject(pid, dll_path)) {
        std::cerr << "Injection failed\n";
        return 1;
    }

    std::cout << "Injected. The DLL will dump schemas and unload itself.\n";
    return 0;
}
