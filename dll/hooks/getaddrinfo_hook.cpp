#include "getaddrinfo_hook.h"
#include "iat.h"
#include "../log.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace ds2sc::hooks::dns {

namespace {

using getaddrinfo_t  = int    (WSAAPI*)(PCSTR, PCSTR, const ADDRINFOA*, PADDRINFOA*);
using gethostbyname_t = struct hostent* (WSAAPI*)(const char*);

getaddrinfo_t   g_orig_getaddrinfo   = nullptr;
gethostbyname_t g_orig_gethostbyname = nullptr;
bool            g_installed          = false;

bool host_endswith(const char* host, const char* suffix) {
    size_t n = std::strlen(host);
    size_t m = std::strlen(suffix);
    return n >= m && _stricmp(host + (n - m), suffix) == 0;
}

bool is_fromsoft_hostname(const char* host) {
    if (!host) return false;
    static const char* deny[] = {
        ".fromsoftware-game.net",
        ".fromsoftware.jp",
    };
    for (auto* s : deny) {
        if (host_endswith(host, s)) return true;
    }
    return false;
}

int WSAAPI hooked_getaddrinfo(PCSTR pNodeName, PCSTR pServiceName,
                              const ADDRINFOA* pHints, PADDRINFOA* ppResult)
{
    if (pNodeName && is_fromsoft_hostname(pNodeName)) {
        char line[256];
        std::snprintf(line, sizeof(line), "getaddrinfo: blocked %s", pNodeName);
        log::info(line);
        if (ppResult) *ppResult = nullptr;
        WSASetLastError(WSAHOST_NOT_FOUND);
        return WSAHOST_NOT_FOUND;
    }
    return g_orig_getaddrinfo(pNodeName, pServiceName, pHints, ppResult);
}

struct hostent* WSAAPI hooked_gethostbyname(const char* name) {
    if (name && is_fromsoft_hostname(name)) {
        char line[256];
        std::snprintf(line, sizeof(line), "gethostbyname: blocked %s", name);
        log::info(line);
        WSASetLastError(WSAHOST_NOT_FOUND);
        return nullptr;
    }
    return g_orig_gethostbyname(name);
}

}

void install() {
    if (g_installed) return;

    HMODULE host = GetModuleHandleW(nullptr);
    if (!host) {
        log::error("dns: GetModuleHandleW(NULL) returned null");
        return;
    }

    bool any = false;
    any |= patch_iat(host, "ws2_32.dll", "getaddrinfo",
                     reinterpret_cast<void*>(&hooked_getaddrinfo),
                     reinterpret_cast<void**>(&g_orig_getaddrinfo));
    any |= patch_iat(host, "ws2_32.dll", "gethostbyname",
                     reinterpret_cast<void*>(&hooked_gethostbyname),
                     reinterpret_cast<void**>(&g_orig_gethostbyname));

    if (!any) {
        log::error("dns: no ws2_32 DNS imports patched");
        return;
    }

    g_installed = true;
    log::info("dns: hooks installed");
}

}
