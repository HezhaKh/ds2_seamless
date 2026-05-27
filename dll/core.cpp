#include "core.h"
#include "log.h"
#include "settings.h"
#include "version_gate.h"
#include "hooks/getaddrinfo_hook.h"
#include "hooks/savefile_hook.h"
#include "hooks/server_redirect_hook.h"
#include "player_count.h"
#include "scaling.h"

#include <windows.h>
#include <psapi.h>
#include <atomic>
#include <fstream>
#include <sstream>
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

    // M3 scaling worker. The param tables aren't loaded at the title screen, so
    // we poll scaling::apply() until it resolves the master param table (return
    // >= 0). apply() handles N==1 (no-op) and the patch itself.
    std::atomic<bool> g_stop_worker{false};

    DWORD WINAPI scaling_worker(LPVOID) {
        for (int attempt = 0; attempt < 1200 && !g_stop_worker.load(); ++attempt) {
            if (scaling::apply() >= 0) return 0;  // param table resolved; done
            Sleep(500);
        }
        if (!g_stop_worker.load())
            log::warn("scaling: gave up waiting for param table (no save loaded?)");
        return 0;
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

    // M4 — redirect save files to a mod-specific extension so vanilla .sl2
    // saves are never touched.
    hooks::savefile::configure(s.save_file_extension);
    hooks::savefile::install();

    // M5a — redirect the game to our co-op coordinator (ds3os). Reads the
    // coordinator's RSA public key from the PEM file next to this DLL. If no
    // coordinator_host is set, this is a no-op and the M2 DNS block keeps the
    // game offline.
    {
        std::string pubkey;
        if (!s.coordinator_host.empty()) {
            std::wstring keyfile(s.coordinator_pubkey_file.begin(), s.coordinator_pubkey_file.end());
            std::ifstream kf(dir + L"\\" + keyfile, std::ios::binary);
            if (kf) {
                std::stringstream ss; ss << kf.rdbuf();
                pubkey = ss.str();
            } else {
                log::warn("server_redirect: coordinator_pubkey_file not found next to DLL");
            }
        }
        hooks::server_redirect::configure(s.coordinator_host, pubkey, s.coordinator_port);
        hooks::server_redirect::install();
    }

    // M3 — per-player enemy HP scaling.
    player_count::set(s.debug_player_count);
    scaling::init(s.enemy_health_scaling, s.boss_health_scaling);
    HANDLE t = CreateThread(nullptr, 0, scaling_worker, nullptr, 0, nullptr);
    if (t) CloseHandle(t);

    log::info("ds2sc: bootstrap done (M2 DNS hooks armed; M3 scaling worker started)");
    return 0;
}

void teardown() {
    g_stop_worker.store(true);
    scaling::restore();
    log::info("ds2sc: teardown");
    log::shutdown();
}

}
