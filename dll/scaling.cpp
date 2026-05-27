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
#include <unordered_set>

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
//        EnemyParam row + 0x28 (u32) = baseHpNg               <- the HP lever
//        BossBattleParam row + 0xC (u32) = ChrParam ID        <- boss -> chr id
// ---------------------------------------------------------------------------
constexpr uintptr_t GAMEMANAGER_RVA = 0x16148f0;

int  g_enemy_pct = 0;
int  g_boss_pct  = 0;

std::mutex g_mtx;
// key = absolute address of a patched HP field; value = original HP.
std::unordered_map<uintptr_t, uint32_t> g_backup;

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

uintptr_t find_param(uintptr_t master, const char* needle) {
    constexpr int MAX_PARAMS = 400;
    for (int i = 0; i < MAX_PARAMS; ++i) {
        const uintptr_t entry = master + 0x40 + static_cast<uintptr_t>(0x18) * i;
        uint32_t data_off, name_off;
        if (!safe_read(entry + 0x10, data_off)) continue;
        if (!safe_read(entry + 0x14, name_off)) continue;
        char name[64];
        if (!safe_read_str(master + name_off, name, sizeof(name))) continue;
        if (std::strstr(name, needle)) return master + data_off;
    }
    return 0;
}

// Iterate a param's rows. Calls fn(rowId, rowAddr) for each. Returns row count.
template <typename Fn>
int iterate_param_rows(uintptr_t param_addr, Fn fn) {
    uint16_t row_count = 0;
    if (!safe_read(param_addr + 0xA, row_count)) return 0;
    int seen = 0;
    for (uint16_t r = 0; r < row_count; ++r) {
        const uintptr_t rentry = param_addr + 0x40 + static_cast<uintptr_t>(0x18) * r;
        uint32_t row_id, row_off;
        if (!safe_read(rentry + 0x00, row_id)) continue;
        if (!safe_read(rentry + 0x08, row_off)) continue;
        fn(row_id, param_addr + row_off);
        ++seen;
    }
    return seen;
}

// The boss chr-id set comes from BossBattleParam: each row's ChrParam ID (+0xC)
// names the boss's character. Empty if the table isn't present (→ no boss
// differentiation, all enemies get the enemy rate; no regression vs cut 1a).
std::unordered_set<uint32_t> collect_boss_ids(uintptr_t master) {
    std::unordered_set<uint32_t> ids;
    const uintptr_t bbp = find_param(master, "BossBattleParam.");
    if (!bbp) return ids;
    iterate_param_rows(bbp, [&](uint32_t /*row_id*/, uintptr_t row_addr) {
        uint32_t chr_id = 0;
        if (safe_read(row_addr + 0xC, chr_id) && chr_id != 0) ids.insert(chr_id);
    });
    return ids;
}

double mul_from(int pct) {
    const int n = player_count::get();
    if (n <= 1 || pct == 0) return 1.0;
    return 1.0 + static_cast<double>(n - 1) * (static_cast<double>(pct) / 100.0);
}

uint32_t scale_u32(uint32_t orig, double mul) {
    const long long s = std::llround(static_cast<double>(orig) * mul);
    return s > 0xffffffffLL ? 0xffffffffu : s < 0 ? 0u : static_cast<uint32_t>(s);
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
        log::warn("scaling: EnemyParam table not found (offsets may be off for this build)");
        return 0;
    }

    const double enemy_mul = mul_from(g_enemy_pct);
    const double boss_mul  = mul_from(g_boss_pct);

    if (enemy_mul == 1.0 && boss_mul == 1.0) {
        if (!g_backup.empty()) {
            for (auto& [addr, orig] : g_backup) safe_write_u32(addr, orig);
            g_backup.clear();
            log::info("scaling: player_count==1 — reverted to vanilla HP");
        }
        return 0;
    }

    const std::unordered_set<uint32_t> boss_ids = collect_boss_ids(master);

    int patched_enemy = 0, patched_boss = 0;
    int hit_by_rowid = 0, hit_by_chrid = 0;  // diagnostic: which key joins bosses

    iterate_param_rows(enemy_param, [&](uint32_t row_id, uintptr_t row_addr) {
        uint32_t chr_id = 0;
        safe_read(row_addr + 0x0, chr_id);  // EnemyParam.chrId

        const bool by_rowid = boss_ids.count(row_id) != 0;
        const bool by_chrid = boss_ids.count(chr_id) != 0;
        if (by_rowid) ++hit_by_rowid;
        if (by_chrid) ++hit_by_chrid;
        // Bosses join by EnemyParam row id (verified live: chrId-field hits = 0).
        // by_chrid is computed for the diagnostic log only, not the decision.
        const bool is_boss = by_rowid;

        const double mul = is_boss ? boss_mul : enemy_mul;
        if (mul == 1.0) return;  // this category isn't being scaled

        const uintptr_t hp_addr = row_addr + 0x28;
        uint32_t cur;
        if (!safe_read(hp_addr, cur)) return;

        uint32_t orig;
        auto it = g_backup.find(hp_addr);
        if (it == g_backup.end()) { orig = cur; g_backup.emplace(hp_addr, cur); }
        else                       { orig = it->second; }
        if (orig == 0) return;

        if (safe_write_u32(hp_addr, scale_u32(orig, mul))) {
            if (is_boss) ++patched_boss; else ++patched_enemy;
        }
    });

    char line[224];
    std::snprintf(line, sizeof(line),
        "scaling: enemy x%.2f (%d rows), boss x%.2f (%d rows); "
        "bossIds=%zu [rowidHits=%d chridHits=%d] N=%d",
        enemy_mul, patched_enemy, boss_mul, patched_boss,
        boss_ids.size(), hit_by_rowid, hit_by_chrid, player_count::get());
    log::info(line);
    return patched_enemy + patched_boss;
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
