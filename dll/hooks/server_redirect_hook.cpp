#include "server_redirect_hook.h"
#include "iat.h"
#include "../log.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ds2sc::hooks::server_redirect {

namespace {

// FromSoft's DS2 login host + the marker bracketing the embedded RSA key. On
// DS2 these are plaintext ASCII in the image (no TEA blob, unlike DS3), so we
// just find and overwrite them. Confirmed against ds3os DS2_ReplaceServerAddressHook.
const char ORIG_HOST[] = "frpg2-steam64-ope-login.fromsoftware-game.net";
const char PEM_BEGIN[]  = "-----BEGIN RSA PUBLIC KEY-----";
const char PEM_END[]    = "-----END RSA PUBLIC KEY-----";

std::string g_host;
std::string g_pubkey;
int         g_login_port = 50050;   // coordinator login port to redirect to
bool        g_installed = false;

// DS2 SOTFS client connects to FromSoft's login server on this hardcoded port.
// (ds3os: DS2=50031.) We rewrite it to the coordinator's login port at connect.
constexpr unsigned short DS2_LOGIN_PORT = 50031;

using connect_t = int (WSAAPI*)(SOCKET, const sockaddr*, int);
connect_t g_orig_connect = nullptr;

struct ModuleRange { uint8_t* base; size_t size; };

ModuleRange host_module() {
    HMODULE h = GetModuleHandleW(nullptr);
    MODULEINFO mi{};
    if (h && GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi)))
        return { reinterpret_cast<uint8_t*>(h), mi.SizeOfImage };
    return { nullptr, 0 };
}

// Find every occurrence of [pat,pat+len) in committed-readable pages of the
// module image. Scans page-aware so an unreadable hole can't fault us.
std::vector<uint8_t*> find_all(const ModuleRange& m, const char* pat, size_t len) {
    std::vector<uint8_t*> hits;
    if (!m.base || len == 0) return hits;
    uint8_t* p   = m.base;
    uint8_t* end = m.base + m.size;
    while (p < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) break;
        uint8_t* region_end = reinterpret_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
        const DWORD prot = mbi.Protect & 0xFF;
        const bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            (prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
             prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY);
        if (readable) {
            uint8_t* scan_end = (region_end < end ? region_end : end);
            if (scan_end > p && static_cast<size_t>(scan_end - p) >= len) {
                for (uint8_t* s = p; s <= scan_end - len; ++s) {
                    if (s[0] == static_cast<uint8_t>(pat[0]) && std::memcmp(s, pat, len) == 0)
                        hits.push_back(s);
                }
            }
        }
        p = region_end;
    }
    return hits;
}

// UTF-16LE bytes of an ASCII string (no terminator).
std::string utf16le(const std::string& s) {
    std::string w;
    w.reserve(s.size() * 2);
    for (char c : s) { w.push_back(c); w.push_back('\0'); }
    return w;
}

// Overwrite [addr, addr+capacity) with `repl` (then NUL + zero-fill the rest),
// flipping the page to RW around the write. Returns false if repl doesn't fit.
bool overwrite(uint8_t* addr, size_t capacity, const std::string& repl) {
    if (repl.size() > capacity) return false;
    DWORD old = 0;
    if (!VirtualProtect(addr, capacity, PAGE_READWRITE, &old)) return false;
    std::memcpy(addr, repl.data(), repl.size());
    std::memset(addr + repl.size(), 0, capacity - repl.size());
    VirtualProtect(addr, capacity, old, &old);
    return true;
}

// connect() hook: rewrite the DS2 login port (50031) to the coordinator's port.
// The address itself was already redirected via the hostname-string patch, so
// here we only fix the port. Everything else passes through untouched.
int WSAAPI hooked_connect(SOCKET s, const sockaddr* name, int namelen) {
    if (name && name->sa_family == AF_INET) {
        auto* in = reinterpret_cast<const sockaddr_in*>(name);
        if (in->sin_port == htons(DS2_LOGIN_PORT)) {
            // Modify in place (matches ds3os); the caller's sockaddr is transient.
            const_cast<sockaddr_in*>(in)->sin_port = htons(static_cast<unsigned short>(g_login_port));
            char line[96];
            std::snprintf(line, sizeof(line),
                "server_redirect: connect port %u -> %d", DS2_LOGIN_PORT, g_login_port);
            log::info(line);
        }
    }
    return g_orig_connect(s, name, namelen);
}

}  // namespace

void configure(const std::string& host, const std::string& pubkey_pem, int login_port) {
    g_host       = host;
    g_pubkey     = pubkey_pem;
    if (login_port > 0 && login_port <= 65535) g_login_port = login_port;
}

void install() {
    if (g_installed) return;

    if (g_host.empty()) {
        log::info("server_redirect: no coordinator_host configured; skipping (M2 DNS block stays active)");
        return;
    }

    ModuleRange m = host_module();
    if (!m.base) { log::error("server_redirect: cannot resolve host module"); return; }

    // --- hostname ---
    // DS2 stores the login host as UTF-16 in .rdata (converted to ASCII at
    // runtime for getaddrinfo); some builds may also have an ASCII copy. Patch
    // whichever is present.
    const size_t host_len = sizeof(ORIG_HOST) - 1;
    if (g_host.size() > host_len) {
        char line[160];
        std::snprintf(line, sizeof(line),
            "server_redirect: coordinator_host too long (%zu > %zu); aborting redirect",
            g_host.size(), host_len);
        log::error(line);
        return;
    }
    int host_patched = 0;
    // ASCII copy (if any)
    for (uint8_t* h : find_all(m, ORIG_HOST, host_len)) {
        if (overwrite(h, host_len, g_host)) ++host_patched;
    }
    // UTF-16LE copy
    const std::string worig = utf16le(ORIG_HOST);
    const std::string whost = utf16le(g_host);
    for (uint8_t* h : find_all(m, worig.data(), worig.size())) {
        if (overwrite(h, worig.size(), whost)) ++host_patched;
    }

    // --- RSA public key ---
    int key_patched = 0;
    if (!g_pubkey.empty()) {
        auto begins = find_all(m, PEM_BEGIN, sizeof(PEM_BEGIN) - 1);
        for (uint8_t* b : begins) {
            // The key region runs from BEGIN to the end of the END marker line.
            auto ends = find_all({ b, m.size - static_cast<size_t>(b - m.base) },
                                 PEM_END, sizeof(PEM_END) - 1);
            if (ends.empty()) continue;
            uint8_t* e = ends.front();
            uint8_t* region_end = e + (sizeof(PEM_END) - 1);
            // include a trailing newline if present in the original
            if (*region_end == '\n') ++region_end;
            const size_t cap = static_cast<size_t>(region_end - b);
            if (overwrite(b, cap, g_pubkey)) ++key_patched;
            else {
                char line[160];
                std::snprintf(line, sizeof(line),
                    "server_redirect: coordinator pubkey too long (%zu > %zu); key not patched",
                    g_pubkey.size(), cap);
                log::warn(line);
            }
        }
    } else {
        log::warn("server_redirect: coordinator_pubkey empty; hostname patched but key left as FromSoft's");
    }

    // --- login port (rewrite 50031 -> coordinator port at connect time) ---
    // connect() is imported from ws2_32 by ordinal (4), so the name-based patch
    // misses it; fall back to the ordinal patch.
    HMODULE host_mod = GetModuleHandleW(nullptr);
    bool connect_hooked = patch_iat(host_mod, "ws2_32.dll", "connect",
        reinterpret_cast<void*>(&hooked_connect),
        reinterpret_cast<void**>(&g_orig_connect));
    if (!connect_hooked) {
        connect_hooked = patch_iat_ordinal(host_mod, "ws2_32.dll", 4,
            reinterpret_cast<void*>(&hooked_connect),
            reinterpret_cast<void**>(&g_orig_connect));
    }

    g_installed = (host_patched > 0);
    char line[224];
    std::snprintf(line, sizeof(line),
        "server_redirect: -> %s:%d  (hostname x%d, RSA key x%d, connect-hook %s)",
        g_host.c_str(), g_login_port, host_patched, key_patched,
        connect_hooked ? "ok" : "FAILED");
    if (host_patched > 0 && connect_hooked) log::info(line); else log::error(line);
}

}  // namespace ds2sc::hooks::server_redirect
