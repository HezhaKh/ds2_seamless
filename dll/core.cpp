#include "core.h"
#include "log.h"
#include "settings.h"
#include "version_gate.h"
#include "hooks/getaddrinfo_hook.h"

#include <windows.h>
#include <psapi.h>
#include <string>

namespace ds2sc::core {

namespace {
    std::wstring module_dir(HMODULE m) {
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(m, buf, MAX_PATH);
        if (n == 0 || n == MAX_PATH) return {};
        std::wstring s(buf, n);
        auto pos = s.find_last_of(L"\\/");
        return pos == std::wstring::npos ? std::wstring{} : s.substr(0, pos);
    }

    // Verify we're attached to the binary we expect. Logs the host exe's name and
    // image base — at M2+ we'll add the SHA-256 hash gate.
    void log_host() {
        HMODULE host = GetModuleHandleW(nullptr);
        wchar_t exe[MAX_PATH] = {0};
        GetModuleFileNameW(host, exe, MAX_PATH);

        char narrow[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, exe, -1, narrow, sizeof(narrow), nullptr, nullptr);

        MODULEINFO mi{};
        GetModuleInformation(GetCurrentProcess(), host, &mi, sizeof(mi));

        char line[1024];
        std::snprintf(line, sizeof(line),
            "host: %s  base=0x%p  size=0x%x",
            narrow, mi.lpBaseOfDll, mi.SizeOfImage);
        log::info(line);
    }
}

DWORD WINAPI bootstrap(LPVOID instance) {
    HMODULE self = static_cast<HMODULE>(instance);
    std::wstring dir = module_dir(self);

    log::init(dir);
    log::info("ds2sc: bootstrap begin");

    log_host();

    Settings s;
    load_settings(dir + L"\\ds2sc_settings.ini", s);

    char line[256];
    std::snprintf(line, sizeof(line),
        "settings: save_ext=%s  invaders=%d  enemy_hp=%d  boss_hp=%d",
        s.save_file_extension.c_str(),
        s.allow_invaders ? 1 : 0,
        s.enemy_health_scaling,
        s.boss_health_scaling);
    log::info(line);

    if (!version_gate::check()) {
        log::warn("ds2sc: bootstrap done (hooks NOT installed — version gate refused)");
        return 0;
    }

    hooks::dns::install();

    log::info("ds2sc: bootstrap done (M2 — DNS hooks armed)");
    return 0;
}

void teardown() {
    log::info("ds2sc: teardown");
    log::shutdown();
}

}
