# M3 — Per-player enemy/boss scaling (DESIGN)

> Status: **design draft, needs one more Ghidra pass before code**.
> Schema is final. Hook chokepoint is not.

## Goal

When N players share a co-op session, scale enemy/boss HP and damage so the
fight isn't trivial. Settings are read from `[SCALING]` in
`ds2sc_settings.ini`. Same convention as DS3SC / ERSC: percent-per-extra-player.

For N=1 (solo / no coop active) the multiplier is 1.0 — vanilla behavior.

## INI schema (final, matches existing settings.ini template)

```ini
[SCALING]
enemy_health_scaling  = 35   ; % per extra player, regular enemies
enemy_damage_scaling  = 0    ; % per extra player, regular enemies
boss_health_scaling   = 100  ; % per extra player, bosses
boss_damage_scaling   = 0    ; % per extra player, bosses
```

**Diff from DS3SC/ERSC**: drop `enemy_posture_scaling` and `boss_posture_scaling`
— DS2 has no posture meter (only poise, which is a stagger-threshold, not a HP-like
bar that scales).

**Multiplier formula** (matches Luke's mods):
```
mul = 1.0 + (N - 1) * (scaling_pct / 100.0)
```

So `enemy_health_scaling = 35`, 4 players → `1.0 + 3 * 0.35 = 2.05x` HP.

## Player-count source

The multiplier needs **N** — the live coop player count. M5 will own this state.
For M3 we expose a debug INI knob so we can verify the hook in isolation:

```ini
[SCALING]
# Debug only — pretend this many players are in the session. M5 will replace
# this with the live coop count. Default 1 = vanilla.
_debug_player_count = 1
```

When M5 lands, `_debug_player_count` is removed and the lobby state feeds the
multiplier directly.

## Boss-vs-enemy detection

From recon (see `recon/strings_scaling.csv` + `chokepoints_m3.txt`):

- There are separate `BossParam` / `BossPartsParam` / `BossEnemyGenerateParam`
  tables. So bosses have their own param rows (not just a flag on `EnemyParam`).
- `BossBattleParam.param` is loaded specifically (`FUN_1404500b0` is the caller of
  the filename-getter at `0x14048bf00`).

This means the simplest boss-test is: **does this enemy come from `BossParam` or
`EnemyParam`?** That's a flag on the ChrIns or determinable from the row source.

## Hook chokepoint (STILL TBD — static analysis exhausted)

We need to find: **the function that initializes a `ChrIns`'s `maxHp` and
`damage` fields by reading from `EnemyParam` / `BossParam`**.

### What pass-2 + pass-3 static analysis told us

The param-resource architecture is clear:

```
ChrParamLoader (FUN_140359bb0)
  └─ iterates 0x52 names from FUN_14048b620
     └─ calls FUN_1402ddb60(mgr, L"param:/X.param", typeId=0x1e, 0)
        = the "resource-by-name" service (see chokepoints_m3_pass3.txt)
           └─ returns a wrapped resource pointer; stores at
              `mgr + 0x310 + 0x10 * index`
```

So the EnemyParam *file* lives at `<chrparam_holder> + 0x310 + 4*0x10 = +0x350`.
But the row-by-id read (`EnemyParam[id].maxHp`) sits behind several vtable
dispatches on the resource wrapper, and the row struct layout isn't recoverable
without RTTI.

The three "sibling 2053-callers" candidates from pass 2 (`FUN_140832f50/e70/f10`)
turned out to be **TLS plumbing**, not param accessors.

### Realistic next step: Cheat Engine live introspection

Static decompilation got us the architecture; CE gets us the precise instruction.
Workflow:

1. Launch the game with `_debug_player_count = 1`, ds2sc M2 hooks armed.
2. CE: attach to `DarkSoulsII.exe`, scan for HP int (e.g. 50 for a Hollow Infantry
   at Things Betwixt).
3. Take damage, scan-decreased; iterate until 1 result.
4. Right-click the address → "Find out what writes this address" (for max-HP
   init we want **reads** instead — "Find out what accesses this address").
5. Trigger a load (warp, save-quit-reload) so the init runs again. CE shows the
   instruction; its containing function is the row-reader.
6. That function's RVA is the M3 hook target. Replace pass-3's TBD with the real
   address, then implement.

After CE pins the address: the hook itself is small (~50 LOC) and we know exactly
how to write it.

Three candidate strategies:

| Strategy | Where to hook | Pros | Cons |
|---|---|---|---|
| **A. EnemyParam row-reader** | The function that pulls the HP/dmg field from a param row | One hook covers all enemies; values flow naturally through the rest of the engine | Hardest to find — need to identify the field offset and the reader |
| **B. ChrIns init / SetMaxHp** | ChrIns constructor or its HP-init method | Conceptually cleanest | ChrIns methods aren't symbol-named; need RTTI or pattern matching |
| **C. Damage-apply** | The function that subtracts inbound damage from a ChrIns's HP | Inverse-scale damage on the way in (no HP edits) | Doesn't scale enemy outbound damage; needs a sibling hook for that |

**Recommendation: A**, fall back to **B** if A's reader is too well-inlined.

**Recon still needed before coding** (CE workflow above is the path):
1. CE-derived RVA of the row-reader that returns `EnemyParam.maxHp` for an
   enemy ID.
2. Confirm the same reader serves both Enemy and Boss param rows (or whether
   bosses go through a separate path — likely via the BossParam table loaded
   into the same `<chrparam_holder> + 0x310 + 0x35*0x10 = +0x660` slot).
3. The field offset of `maxHp` inside the returned row (CE shows this directly
   in the instruction operand).

Once those three numbers are known: hook = `~50 LOC`, multiply two integers
and return.

## What we do NOT do at M3

| Skipped | Why |
|---|---|
| Posture scaling | DS2 has no posture system. |
| Stat scaling at the regulation-bin level | Brittle — would need to re-pack `enc_regulation.bnd.dcx`, and changes leak into solo play / other saves. |
| Per-player friendly-fire / damage knobs | M5+ work — needs the lobby's player table. |
| Boss-room kick / proximity rules | M5+ work — lobby state. |

## Implementation files (planned)

```
dll/
  hooks/
    scaling.h, scaling.cpp     # the per-player multiplier hook
  player_count.h, .cpp         # holds N; M3 reads _debug_player_count, M5 overrides
```

New CMake link libs: none (still pure Win32).

## Acceptance criteria

- [ ] `_debug_player_count = 1` → enemy HP matches vanilla (sanity check).
- [ ] `_debug_player_count = 4`, `enemy_health_scaling = 100` → regular enemy
      visibly takes ~4× as many hits to kill (verify against a known mob, e.g.
      first hollow in Things Betwixt).
- [ ] `_debug_player_count = 4`, `boss_health_scaling = 100` → boss HP bar
      empties ~4× slower (verify against The Pursuer / Last Giant).
- [ ] `enemy_damage_scaling = 100`, 4 players → mob hits do ~4× damage.
- [ ] No crash on map transition / boss fog entry / save-and-quit.
- [ ] Log shows multiplier values applied at session start.

## After M3

M4 (save split `.sl2` → `.co2`) is the next milestone. The save chokepoint is
already known: `0x140a8739e` (in `FUN_140a87110`, `SaveLoad2::SLContentFormat`
ctor — see `chokepoints.txt`). M3 and M4 are independent.
