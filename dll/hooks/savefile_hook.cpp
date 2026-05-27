#include "savefile_hook.h"
#include "iat.h"
#include "../log.h"

#include <windows.h>
#include <string>
#include <atomic>
#include <cwctype>
#include <cctype>

namespace ds2sc::hooks::savefile {

namespace {

using CreateFileW_t = HANDLE (WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFileA_t = HANDLE (WINAPI*)(LPCSTR,  DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

CreateFileW_t g_orig_w = nullptr;
CreateFileA_t g_orig_a = nullptr;
bool          g_installed = false;

// DS2 opens the save container many times per save cycle. The redirect must
// run on every open, but we only log the first one of each kind to keep the
// log readable.
std::atomic<bool> g_logged_w{false};
std::atomic<bool> g_logged_a{false};

std::wstring g_ext_w = L"co2";   // no leading dot
std::string  g_ext_a = "co2";

// The vanilla extension we redirect away from.
constexpr wchar_t SL2_W[] = L".sl2";
constexpr char    SL2_A[] = ".sl2";

std::wstring to_lower(std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; }
std::string  to_lower(std::string  s) { for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c))); return s; }

// Replace every case-insensitive occurrence of ".sl2" with "." + ext. Handles
// both "X.sl2" and a hypothetical "X.sl2.bak" consistently.
std::wstring rewrite(const std::wstring& path) {
    const std::wstring repl = L"." + g_ext_w;
    std::wstring out = path, low = to_lower(path);
    size_t pos = 0;
    while ((pos = low.find(SL2_W, pos)) != std::wstring::npos) {
        out.replace(pos, 4, repl);
        low.replace(pos, 4, repl);   // repl is already lowercase (ascii ext)
        pos += repl.size();
    }
    return out;
}
std::string rewrite(const std::string& path) {
    const std::string repl = "." + g_ext_a;
    std::string out = path, low = to_lower(path);
    size_t pos = 0;
    while ((pos = low.find(SL2_A, pos)) != std::string::npos) {
        out.replace(pos, 4, repl);
        low.replace(pos, 4, repl);
        pos += repl.size();
    }
    return out;
}

bool is_save(const std::wstring& p) { return to_lower(p).find(SL2_W) != std::wstring::npos; }
bool is_save(const std::string&  p) { return to_lower(p).find(SL2_A) != std::string::npos; }

// Log just the filename leaf to keep lines short and avoid dumping the full
// user path on every save.
const wchar_t* leaf(const wchar_t* p) {
    const wchar_t* s = p;
    for (const wchar_t* c = p; *c; ++c) if (*c == L'\\' || *c == L'/') s = c + 1;
    return s;
}

void log_redirect_w(const wchar_t* from, const wchar_t* to) {
    char buf[256];
    int n = WideCharToMultiByte(CP_UTF8, 0, leaf(from), -1, buf, 200, nullptr, nullptr);
    std::string a = (n > 0) ? buf : "?";
    n = WideCharToMultiByte(CP_UTF8, 0, leaf(to), -1, buf, 200, nullptr, nullptr);
    std::string b = (n > 0) ? buf : "?";
    log::info(("savefile: redirect " + a + " -> " + b).c_str());
}

HANDLE WINAPI hooked_w(LPCWSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa,
                       DWORD disp, DWORD flags, HANDLE tmpl) {
    if (name) {
        std::wstring p(name);
        if (is_save(p)) {
            std::wstring rw = rewrite(p);
            if (!g_logged_w.exchange(true)) log_redirect_w(name, rw.c_str());
            return g_orig_w(rw.c_str(), acc, share, sa, disp, flags, tmpl);
        }
    }
    return g_orig_w(name, acc, share, sa, disp, flags, tmpl);
}

HANDLE WINAPI hooked_a(LPCSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa,
                       DWORD disp, DWORD flags, HANDLE tmpl) {
    if (name) {
        std::string p(name);
        if (is_save(p)) {
            std::string rw = rewrite(p);
            if (!g_logged_a.exchange(true)) log::info(("savefile: redirect(A) -> " + rw).c_str());
            return g_orig_a(rw.c_str(), acc, share, sa, disp, flags, tmpl);
        }
    }
    return g_orig_a(name, acc, share, sa, disp, flags, tmpl);
}

}  // namespace

void configure(const std::string& extension) {
    if (extension.empty()) return;
    g_ext_a = extension;
    g_ext_w.assign(extension.begin(), extension.end());  // save extensions are ASCII
}

void install() {
    if (g_installed) return;
    HMODULE host = GetModuleHandleW(nullptr);
    if (!host) { log::error("savefile: GetModuleHandleW(NULL) null"); return; }

    bool any = false;
    any |= patch_iat(host, "KERNEL32.DLL", "CreateFileW",
                     reinterpret_cast<void*>(&hooked_w),
                     reinterpret_cast<void**>(&g_orig_w));
    any |= patch_iat(host, "KERNEL32.DLL", "CreateFileA",
                     reinterpret_cast<void*>(&hooked_a),
                     reinterpret_cast<void**>(&g_orig_a));
    if (!any) {
        log::error("savefile: no CreateFile imports patched");
        return;
    }
    g_installed = true;
    log::info(("savefile: hooks installed (.sl2 -> ." + g_ext_a + ")").c_str());
}

}  // namespace ds2sc::hooks::savefile
