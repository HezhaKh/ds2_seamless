#include "scaling.h"
#include "player_count.h"
#include "log.h"

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace ds2sc::scaling {

namespace {

// ---------------------------------------------------------------------------
// Memory map (verified against the boblord14 SOTFS cheat table's Param Patcher
// v2 and cross-checked with our own Ghidra recon: GameManagerImp+0x30 == the
// param repository at DAT_1416148f0+0x30 used by FUN_1402ddb60).
//
//   GameManagerImp static pointer : image RVA 0x16148f0  (== DAT_1416148f0)
//   MASTER_PARAM_TABLE            : [[[[GameManagerImp]+30]+118]+D8]+0
//   param index entry (0x18 wide) : MASTER+0x40 + 0x18*i
//        entry+0x10 (u32) -> param data-block offset (rel. MASTER)
//        entry+0x14 (u32) -> name-string offset      (rel. MASTER)
//   per-param row table           : ParamAddr = MASTER + dataOffset
//        rowCount = u16 at ParamAddr+0xA
//        row index (0x18 wide) : ParamAddr+0x40 + 0x18*r
//             +0x00 (u32) row id
//             +0x08 (u32) row data offset (rel. ParamAddr)
//        EnemyParam row + 0x28 (u32) = baseHpNg   <- the HP lever
// ---------------------------------------------------------------------------
constexpr uintptr_t GAMEMANAGER_RVA = 0x16148f0;

int  g_enemy_pct = 0;
int  g_boss_pct  = 0;  // reserved for cut 1b (boss differentiation)

std::mutex g_mtx;
// key = absolute address of the patched HP field; value = original HP.
std::unordered_map<uintptr_t, uint32_t> g_backup;

// Page-validated read. Returns false (without dereferencing) if `addr` is not
// in committed, readable memory big enough to hold a T. Makes the whole pointer
// walk crash-proof even if an offset is wrong.
template <typename T>
bool safe_read(uintptr_t addr, T& out) {
    if (addr == 0) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    const DWORD base = mbi.Protect & 0xFF;
    const bool readable =
        base == PAGE_READONLY || base == PAGE_READWRITE ||
        base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READ ||
        base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
    if (!readable) return false;
    const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (addr + sizeof(T) > region_end) return false;
    out = *reinterpret_cast<T*>(addr);
    return true;
}

// Read a printable C-string; bail if it hits non-printable bytes (garbage) or
// has no terminator within `cap`. Used to validate param-name entries.
bool safe_read_str(uintptr_t addr, char* out, size_t cap) {
    for (size_t i = 0; i + 1 < cap; ++i) {
        char c;
        if (!safe_read(addr + i, c)) { out[i] = 0; return false; }
        out[i] = c;
        if (c == 0) return true;
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7e) { out[i] = 0; return false; }
    }
    out[cap - 1] = 0;
    return false;
}

bool safe_write_u32(uintptr_t addr, uint32_t val) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (addr + sizeof(uint32_t) > region_end) return false;

    const DWORD base = mbi.Protect & 0xFF;
    const bool writable =
        base == PAGE_READWRITE || base == PAGE_EXECUTE_READWRITE ||
        base == PAGE_WRITECOPY || base == PAGE_EXECUTE_WRITECOPY;
    if (writable) {
        *reinterpret_cast<uint32_t*>(addr) = val;
        return true;
    }
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), sizeof(uint32_t), PAGE_READWRITE, &old))
        return false;
    *reinterpret_cast<uint32_t*>(addr) = val;
    VirtualProtect(reinterpret_cast<LPVOID>(addr), sizeof(uint32_t), old, &old);
    return true;
}

uintptr_t resolve_master_param_table() {
    const uintptr_t mod = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!mod) return 0;
    uintptr_t instance, resmgr, p2, p3;
    if (!safe_read(mod + GAMEMANAGER_RVA, instance) || !instance) return 0;
    if (!safe_read(instance + 0x30, resmgr) || !resmgr) return 0;
    if (!safe_read(resmgr + 0x118, p2) || !p2) return 0;
    if (!safe_read(p2 + 0xD8, p3) || !p3) return 0;
    return p3;  // MASTER_PARAM_TABLE = ...+0
}

// Find a param table's data-block address by name. Iterates the param index,
// validating each name string. Returns 0 if not found.
uintptr_t find_param(uintptr_t master, const char* needle) {
    constexpr int MAX_PARAMS = 400;   // DS2 has ~230; generous cap
    for (int i = 0; i < MAX_PARAMS; ++i) {
        const uintptr_t entry = master + 0x40 + static_cast<uintptr_t>(0x18) * i;
        uint32_t data_off, name_off;
        if (!safe_read(entry + 0x10, data_off)) continue;
        if (!safe_read(entry + 0x14, name_off)) continue;
        char name[64];
        if (!safe_read_str(master + name_off, name, sizeof(name))) continue;
        if (std::strstr(name, needle)) {
            return master + data_off;
        }
    }
    return 0;
}

double enemy_multiplier() {
    const int n = player_count::get();
    if (n <= 1 || g_enemy_pct == 0) return 1.0;
    return 1.0 + static_cast<double>(n - 1) * (static_cast<double>(g_enemy_pct) / 100.0);
}

}  // namespace

void init(int enemy_health_pct, int boss_health_pct) {
    g_enemy_pct = enemy_health_pct;
    g_boss_pct  = boss_health_pct;
}

int apply() {
    std::lock_guard<std::mutex> lock(g_mtx);

    const uintptr_t master = resolve_master_param_table();
    if (!master) return -1;  // not ready yet

    const uintptr_t enemy_param = find_param(master, "EnemyParam.");
    if (!enemy_param) {
        // Master resolved but the table name didn't match — offsets are likely
        // wrong for this build. Return "done" (not "not ready") so the poll
        // worker stops instead of spamming this warning.
        log::warn("scaling: EnemyParam table not found (offsets may be off for this build)");
        return 0;
    }

    const double mul = enemy_multiplier();
    if (mul == 1.0) {
        // N==1 or 0% — nothing to scale. Make sure we aren't leaving a stale patch.
        if (!g_backup.empty()) {
            for (auto& [addr, orig] : g_backup) safe_write_u32(addr, orig);
            g_backup.clear();
            log::info("scaling: player_count==1 — reverted to vanilla HP");
        }
        return 0;
    }

    uint16_t row_count = 0;
    if (!safe_read(enemy_param + 0xA, row_count) || row_count == 0) {
        log::warn("scaling: EnemyParam row count unreadable");
        return 0;
    }

    int patched = 0;
    for (uint16_t r = 0; r < row_count; ++r) {
        const uintptr_t rentry = enemy_param + 0x40 + static_cast<uintptr_t>(0x18) * r;
        uint32_t row_off;
        if (!safe_read(rentry + 0x08, row_off)) continue;
        const uintptr_t hp_addr = enemy_param + row_off + 0x28;

        uint32_t cur;
        if (!safe_read(hp_addr, cur)) continue;

        uint32_t orig;
        auto it = g_backup.find(hp_addr);
        if (it == g_backup.end()) { orig = cur; g_backup.emplace(hp_addr, cur); }
        else                       { orig = it->second; }

        if (orig == 0) continue;  // skip rows with no HP (props / non-combat)
        const long long scaled_ll = std::llround(static_cast<double>(orig) * mul);
        const uint32_t scaled = scaled_ll > 0xffffffffLL ? 0xffffffffu
                              : scaled_ll < 0            ? 0u
                                                         : static_cast<uint32_t>(scaled_ll);
        if (safe_write_u32(hp_addr, scaled)) ++patched;
    }

    char line[160];
    std::snprintf(line, sizeof(line),
        "scaling: EnemyParam x%.2f applied to %d/%u rows (N=%d, %d%%/player)",
        mul, patched, row_count, player_count::get(), g_enemy_pct);
    log::info(line);
    return patched;
}

void restore() {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_backup.empty()) return;
    int n = 0;
    for (auto& [addr, orig] : g_backup) {
        if (safe_write_u32(addr, orig)) ++n;
    }
    g_backup.clear();
    char line[96];
    std::snprintf(line, sizeof(line), "scaling: restored %d rows to vanilla HP", n);
    log::info(line);
}

}  // namespace ds2sc::scaling
