# M3 — Per-player enemy/boss HP scaling

> Status: **cut 1a (enemy HP) + cut 1b (boss-rate) implemented + verified on hardware.**
> Damage scaling and live player count (M5) remain.

## Goal

With N co-op players, scale enemy HP so fights aren't trivial. Mirrors
DS3SC/ERSC `[SCALING]` (percent-per-extra-player), minus posture (DS2 has none).
`mul = 1 + (N-1) * pct/100`. N=1 → vanilla.

## Approach: runtime param-patch (chosen over code hooks)

DS2 loads its param tables once at boot and keeps them resident through
save-load (verified: a patch applied at the title screen was still in effect
after loading a save). So we multiply the HP field directly in the `EnemyParam`
rows in memory at session start, snapshot originals for exact restore, and never
inject executable code. Lower-risk than hooking the spawn-init instruction.

This mechanism + its offsets came from the **boblord14 DS2 SOTFS Cheat Table
(Bob Edition)** "Param Patcher v2" — community RE of the *base game*, which is
within our clean-room rule (we never disassemble Luke's coop DLLs). It
cross-validates our own Ghidra recon: the CT's `[[GameManagerImp]+30]` param
repository is the same object our Ghidra found at `*(DAT_1416148f0+0x30)` in
`FUN_1402ddb60`. We wrote 100% of the C++ ourselves; no CT code was copied.

## Verified memory map

```
GameManagerImp static pointer : image RVA 0x16148f0   (== Ghidra DAT_1416148f0)
MASTER_PARAM_TABLE            : [[[[GameManagerImp]+30]+118]+D8]+0
param index entry (0x18 wide) : MASTER + 0x40 + 0x18*i
     entry+0x10 (u32) = param data-block offset (rel. MASTER)
     entry+0x14 (u32) = name-string offset      (rel. MASTER)  e.g. "EnemyParam.param"
per-param row table           : ParamAddr = MASTER + dataOffset
     rowCount = u16 at ParamAddr+0xA
     row index (0x18 wide) : ParamAddr + 0x40 + 0x18*r
          +0x00 (u32) row id
          +0x08 (u32) row data offset (rel. ParamAddr)
     EnemyParam row + 0x28 (u32) = baseHpNg     <- the HP lever
```

For live cross-check only (not patched in this approach):
```
ChrIns + 0x168 (u32) = current HP
ChrIns + 0x170 (u32) = max HP    (player via GameManagerImp->+0xD0->+0x168/+0x170)
```

## Implementation (shipped)

- `dll/player_count.{h,cpp}` — atomic source of truth for N. M3 seeds from
  `_debug_player_count`; M5 will override from the live lobby.
- `dll/scaling.{h,cpp}` — param-patch engine:
  - `resolve_master_param_table()` walks the pointer chain; returns 0 until
    params are resident.
  - `find_param("EnemyParam.")` iterates the index, validates name strings.
  - `apply()` multiplies `baseHpNg` (0x28) in every EnemyParam row, snapshotting
    each original on first touch (keyed by HP-field address) so re-apply never
    compounds. Returns rows patched, or -1 if the table isn't resident yet.
  - `restore()` writes originals back.
  - All reads/writes go through page-validated helpers (`VirtualQuery` guard) so
    a wrong offset degrades to "no scaling + log line", never a crash.
- `dll/core.cpp` — after M2 hooks, seeds player_count, `scaling::init(...)`, and
  spawns a worker that polls `apply()` every 500ms until the table resolves
  (params load ~seconds after boot). `teardown()` calls `restore()`.
- `dll/settings.{h,cpp}` + INI — added `_debug_player_count` (default 1).
  Posture knobs dropped (DS2 has none); damage knobs parsed but unused this cut.

### Verification result (2026-05-26)
```
scaling: EnemyParam x4.00 applied to 976/976 rows (N=4, 100%/player)
```
With `_debug_player_count=4`, `enemy_health_scaling=100`: Majula pigs visibly
~4× tankier, persisting through save-load. No crash; clean teardown.

## Cut 1b — boss-rate differentiation (implemented + verified)

Bosses are `EnemyParam` rows (no param-level boss flag; `EnemyCommonParam` has
no isBoss, "Enemy Type" is only Small/Medium/Large). The boss set is derived
**from the data**, not hardcoded: each `BossBattleParam` row carries a
`ChrParam ID` at row+0xC that names the boss's character. `collect_boss_ids()`
enumerates BossBattleParam and gathers those IDs; an EnemyParam row is a boss
iff its **row id** is in that set. Boss rows get `boss_health_scaling`, the rest
get `enemy_health_scaling`.

### Verification result (2026-05-26)
With `enemy_health_scaling=0`, `boss_health_scaling=100`, `_debug_player_count=4`:
```
scaling: enemy x1.00 (0 rows), boss x4.00 (33 rows); bossIds=39 [rowidHits=33 chridHits=0] N=4
```
- `bossIds=39` from BossBattleParam (DS2 SOTFS has ~40 bosses).
- Join key is the EnemyParam **row id** (33 matched by row-id, 0 by chrId-field),
  so the decision uses row-id only; the chrId count is kept for diagnostics.
- 33 boss rows scaled, 0 mob rows touched — exactly the intended split.

The 6 unmatched (39→33) are multi-phase / duo battle entries (e.g. Throne
Watcher+Defender) whose `ChrParam ID` has no own EnemyParam row; they fall back
to the enemy rate. Acceptable; revisit only if a specific boss is noticeably
under-scaled.

## Out of scope (later)

- Damage scaling (defaults to 0 in Luke's mods; enemy attack is via weapon/
  AttackParam — separate field-mapping).
- Live lobby player count (M5).
- Param reload edge cases (NG+ regulation reload) — not observed; revisit if seen.

## Attribution

Offsets/mechanism referenced from: **boblord14** (Bob Edition CT),
**Igromanru / Flightplan** (Param Patcher lineage), **Radai**, **Atvaark**.
All C++ here is original; cross-validated against our own Ghidra DB.
```
https://github.com/boblord14/Dark-Souls-2-SotFS-CT-Bob-Edition
```
