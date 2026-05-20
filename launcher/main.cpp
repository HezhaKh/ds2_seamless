// ds2sc_launcher — starts DarkSoulsII.exe suspended, injects ds2sc.dll, resumes.
//
// Layout (matches reference seamless-coop mods):
//   <game>/DarkSoulsII.exe
//   <game>/ds2sc_launcher.exe          <-- this binary
//   <game>/SeamlessCoop/ds2sc.dll
//   <game>/SeamlessCoop/ds2sc_settings.ini

#include <windows.h>
#include <string>

namespace {

std::wstring exe_dir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf, n);
    auto pos = s.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring{} : s.substr(0, pos);
}

bool file_exists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

void fatal(const wchar_t* msg) {
    MessageBoxW(nullptr, msg, L"ds2sc_launcher", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

// Classic LoadLibraryW remote-thread injection. DS2 has no anti-debug/anti-injection
// of consequence so this is sufficient for M1.
bool inject(HANDLE process, const std::wstring& dll_path) {
    SIZE_T bytes = (dll_path.size() + 1) * sizeof(wchar_t);

    LPVOID remote = VirtualAllocEx(process, nullptr, bytes,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return false;

    if (!WriteProcessMemory(process, remote, dll_path.c_str(), bytes, nullptr)) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(k32, "LoadLibraryW"));
    if (!load) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }

    HANDLE rt = CreateRemoteThread(process, nullptr, 0, load, remote, 0, nullptr);
    if (!rt) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(rt, INFINITE);
    DWORD remote_module = 0;
    GetExitCodeThread(rt, &remote_module);
    CloseHandle(rt);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);

    // LoadLibraryW returns the HMODULE in the low 32 bits when called via this
    // shim. A non-zero value means the DLL was loaded successfully.
    return remote_module != 0;
}

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    const std::wstring here     = exe_dir();
    const std::wstring game_exe = here + L"\\DarkSoulsII.exe";
    const std::wstring dll      = here + L"\\SeamlessCoop\\ds2sc.dll";

    if (!file_exists(game_exe))
        fatal(L"DarkSoulsII.exe not found next to ds2sc_launcher.exe. "
              L"Place this launcher in the DS2 SOTFS Game directory.");

    if (!file_exists(dll))
        fatal(L"SeamlessCoop\\ds2sc.dll not found next to the launcher.");

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmdline = L"\"" + game_exe + L"\"";

    BOOL ok = CreateProcessW(
        game_exe.c_str(),
        cmdline.data(),
        nullptr, nullptr, FALSE,
        CREATE_SUSPENDED,
        nullptr,
        here.c_str(),   // launch with cwd = Game/ so the game finds its assets
        &si, &pi);

    if (!ok)
        fatal(L"Failed to start DarkSoulsII.exe (CreateProcess).");

    if (!inject(pi.hProcess, dll)) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        fatal(L"Failed to inject ds2sc.dll.");
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
