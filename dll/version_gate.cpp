#include "version_gate.h"
#include "log.h"

#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ds2sc::version_gate {

namespace {

// SHA-256 of DarkSoulsII.exe for SOTFS v1.0.3.0 (Steam appid 335300).
constexpr uint8_t EXPECTED_SHA256[32] = {
    0x00,0x45,0x93,0x1B, 0x89,0x14,0x50,0x45,
    0x31,0xB7,0x86,0x4A, 0x94,0x88,0xD3,0x96,
    0xDC,0x50,0xCB,0xAF, 0x52,0x49,0x64,0x01,
    0x6E,0x1D,0x69,0xC3, 0xD1,0x17,0x31,0x31,
};

void hex_str(const uint8_t* h, char* out64) {
    static const char* d = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out64[i * 2]     = d[(h[i] >> 4) & 0xF];
        out64[i * 2 + 1] = d[ h[i]       & 0xF];
    }
    out64[64] = 0;
}

// Compute SHA-256 of the file at `path`. Returns true on success.
bool sha256_file(const wchar_t* path, uint8_t out[32]) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        log::error("version_gate: cannot open host exe for hashing");
        return false;
    }

    BCRYPT_ALG_HANDLE  alg  = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0)
    {
        constexpr DWORD CHUNK = 64 * 1024;
        uint8_t buf[CHUNK];
        DWORD read = 0;
        ok = true;
        while (ReadFile(f, buf, CHUNK, &read, nullptr) && read > 0) {
            if (BCryptHashData(hash, buf, read, 0) != 0) { ok = false; break; }
        }
        if (ok && BCryptFinishHash(hash, out, 32, 0) != 0) ok = false;
    }

    if (hash) BCryptDestroyHash(hash);
    if (alg)  BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    return ok;
}

}

bool check() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n == MAX_PATH) {
        log::error("version_gate: cannot resolve host exe path");
        return false;
    }

    uint8_t got[32];
    if (!sha256_file(path, got)) {
        log::error("version_gate: hashing failed");
        return false;
    }

    char got_hex[65];
    hex_str(got, got_hex);

    if (std::memcmp(got, EXPECTED_SHA256, 32) != 0) {
        char line[256];
        std::snprintf(line, sizeof(line),
            "version_gate: SHA-256 mismatch — host exe is not SOTFS v1.0.3.0 (got %s); refusing to install hooks",
            got_hex);
        log::error(line);
        return false;
    }

    char line[128];
    std::snprintf(line, sizeof(line), "version_gate: SHA-256 OK (%s)", got_hex);
    log::info(line);
    return true;
}

}
