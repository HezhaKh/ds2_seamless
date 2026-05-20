#include "core.h"

#include <windows.h>

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        // DllMain runs under the loader lock — do real work in a worker thread.
        HANDLE t = CreateThread(nullptr, 0, ds2sc::core::bootstrap, hModule, 0, nullptr);
        if (t) CloseHandle(t);
        break;
    }
    case DLL_PROCESS_DETACH:
        ds2sc::core::teardown();
        break;
    }
    return TRUE;
}
