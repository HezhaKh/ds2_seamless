# M5b — Seamless presence (DESIGN / feasibility)

> Status: **research + feasibility pass.** No code yet.
> Verdict: **feasible**, foundational technique is publicly documented and we
> already hold the recon anchors. Full seamless is large and built in phases;
> every phase needs a 2nd player to verify.

## What "seamless" means (vs M5a's native co-op)

M5a restored *native* co-op: partners summon via signs and get **desummoned**
("sent home") on boss death, host death, and some area transitions, with fog
walls blocking multiplayer progress. Seamless = partners stay persistently
in-world through all of that.

## Feasibility — the core technique is known

The public one-liner ("block disconnect messages") is an **oversimplification**.
The open-source `scheissgeist/Seamless` (MIT) documents the real, hard-won
mechanism in `docs/DS2_PHANTOM_REMOVAL_RESEARCH.md`; its sources are ds3os + the
Bob CT + DS2S-META — the same RE lineage we already use. Findings:

**Boss kill triggers a cascade of protobuf messages**, not one "send-home" call.
The *client itself* emits `RequestNotifyKillEnemy` (0x03F6) → `RequestRemoveSign`
(0x0396) → `RequestNotifyLeaveGuestPlayer` (0x03E9) → `RequestNotifyLeaveSession`
(0x03EB) → `RequestNotifyDisconnectSession` (0x03F9); the server pushes
`PushRequestRemoveSign`/`PushRequestRejectSign`. Phantoms also have a separate
**timer** (`AllottedTime`: 4000 s white / 500 s shade, shortened by kills).

**The working defense is multi-layer and CRUCIALLY does NOT block outgoing
messages** (blocking outgoing corrupts the server's protobuf stream → breaks the
session; this was a documented bug they hit):

1. **Incoming block** — inline-hook `ParseFromArray`; return false for incoming
   disconnect pushes, identified by **RTTI class name**: `DisconnectSession`,
   `LeaveGuestPlayer`, `LeaveSession`, `BanishPlayer`, `BreakInTarget`,
   `RemovePlayer`, `RemoveSign`, `RejectSign`. `SerializeWithCachedSizesToArray`
   (outgoing) is hooked for *identification/logging only*, never blocking.
2. **Timer refresh** — every ~5 s write `99999.0f` to `AllottedTime`.
3. **Permission patches** — make the local phantom act like a host (bonfires,
   fog, NPCs).

Message type is read via **RTTI**: the hook reads the decorated class name (e.g.
`.?AVRequestNotifyDisconnectSession@@`) off the message object's vtable.

**Implementation prerequisite:** these are *internal* game functions, so M5b
needs an **inline/trampoline hook** (MinHook-style) — our hooks so far are
IAT-only. That infra is M5b's first real task.

### Verified memory map (NetSessionManager chain; Bob CT + scheissgeist)
```
NetSessionManager -> +0x18           = active co-op session
                  -> +0x18 -> +0x17C = AllottedTime (float, seconds)  [timer refresh]
                  -> +0x20           = local player (session context)
                  -> +0x20 -> +0x1F4 = PhantomType (u32: 0=Host)       [zero -> host perms]
                  -> +0x20 -> +0x234 = PlayerName (wchar_t[32])
TeamType: 513=WhitePhantom -> write 0=Host; writer at DarkSoulsII.exe+0xDF1719
ChrNetworkPhantomId: GameManagerImp -> +0xD0 -> +0xB0 -> +0x3C  (zero -> solid look)
Bonfire bits:        GameManagerImp -> +0xD0 -> +0xB8 -> +0x4C8 (set bits 4+5)
```
AOB patterns for `SerializeWithCachedSizesToArray` (76 B, 7 wildcards) and
`ParseFromArray` (20 B) are in scheissgeist `include/addresses.h` (from ds3os).
Cross-check against our Ghidra (`FUN_1406b77e0` protobuf anchor;
`RequestDisconnectUser` string at `140c824fd`).

**Note:** LukeYui is also building DS2 seamless (announced, prototype, unreleased,
will ship binary-only → stays off-limits). Per [[project-ds2sc-scope]] we keep
building and "compare and optimise" if/when his ships.

## Phasing (each phase needs 2 players to verify)

### M5b.−1 — Inline-hook infra (prerequisite, solo-doable)
**Decided (2026-05-27): use MinHook** (small MIT x64 inline-hook lib) — handles
instruction-length decoding / trampolines correctly; accepted breaking the
"no third-party deps" streak for this. Vendor it under `third_party/minhook`,
add to CMake. Also add an AOB pattern scanner (port the scheissgeist/ds3os
patterns from `addresses.h`). No game behaviour yet; unit-test by hooking a known
function and confirming the trampoline returns correctly.

### M5b.0 — Keep phantoms past boss kill (foundational)
Inline-hook `ParseFromArray`; block the incoming disconnect-push messages above
(by RTTI name). Add the `AllottedTime` timer refresh. **Do not** block outgoing.
- **First verifiable sub-goal:** summon a friend, kill a boss (e.g. Last Giant) —
  the phantom stays instead of being sent home.

### M5b.1 — Phantom host permissions
Apply the TeamType/PhantomType permission patches so the phantom can use
bonfires, open fog walls, and interact like a host.

### M5b.2 — Follow on warp / area transition
Re-establish the phantom in the host's new map on bonfire warp / fog transition
(re-summon-on-transition via the coordinator).

### M5b.3 — Progress / world-state sync (hardest, optional)
Shared boss kills, events, progression. Toggleable.

## M3 hookup (lands with M5b once sessions persist)
With partners persistently in-world, read the live phantom count and call
`player_count::set(N)` so HP scaling reflects the real co-op size — replacing
`_debug_player_count`.

## Clean-room ruling (2026-05-27)

User ruled: `scheissgeist/Seamless` (MIT, open-source) **may be referenced like
ds3os / the cheat table** — read for offsets, message IDs, and approach; we still
write 100% of our own code. Only **LukeYui's closed binary DLLs** remain
off-limits. Recorded in [[feedback-cleanroom-rule]].

## Risks / honesty

| Risk | Note |
|---|---|
| Full seamless is a large, multi-phase RE effort | M5b.0 (desummon block) is a real, bounded first win; later phases are progressively harder. |
| Every phase needs a 2nd player to verify | Can't validate solo; pair testing required throughout. |
| Area-follow + sync may need deep session work | Scope each phase with its own recon before coding. |
| Luke's DS2 seamless release | May change priorities; our value is a private, self-hosted, personal version we control. |

## Reference sources
- Public reporting on DS2 seamless technique (disconnect-message blocking).
- Our own recon: [[project-ds2-engine]], `recon/chokepoints.txt`.
- ds3os ([[reference-ds3os]]) coordinator for the session transport.
