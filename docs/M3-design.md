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

## Hook chokepoint (STILL TBD — needs one more recon pass)

We need to find: **the function that initializes a `ChrIns`'s `maxHp` and
`damage` fields by reading from `EnemyParam` / `BossParam`**.

Three candidate strategies:

| Strategy | Where to hook | Pros | Cons |
|---|---|---|---|
| **A. EnemyParam row-reader** | The function that pulls the HP/dmg field from a param row | One hook covers all enemies; values flow naturally through the rest of the engine | Hardest to find — need to identify the field offset and the reader |
| **B. ChrIns init / SetMaxHp** | ChrIns constructor or its HP-init method | Conceptually cleanest | ChrIns methods aren't symbol-named; need RTTI or pattern matching |
| **C. Damage-apply** | The function that subtracts inbound damage from a ChrIns's HP | Inverse-scale damage on the way in (no HP edits) | Doesn't scale enemy outbound damage; needs a sibling hook for that |

**Recommendation: A**, fall back to **B** if A's reader is too well-inlined.

**Recon still needed before coding**:
1. Identify `EnemyParam` row struct layout (offset of `maxHp`, `physicsAttackPower`).
   - Strategy: find the BinderTPL/param-row-by-ID lookup function and look at what
     fields are accessed near the call sites.
2. Identify the function that copies those fields into a ChrIns instance.
   - Strategy: from `FUN_140359bb0` (caller of the param-name registry), walk
     toward the row-bytes-to-ChrIns code.
3. Cross-check with `FUN_1404500b0` (the BossBattleParam loader) to see if it
   provides a clean "is this entity in a boss arena" oracle.

Once that's nailed down, the hook is small (~50 LOC) — multiply a couple of
floats / ints and return.

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
