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

Public reporting on the in-development DS2 seamless mods describes the
foundational trick plainly:

> "hooks the game's protobuf serialization layer to intercept and **block
> disconnect messages**. When the game tries to end your co-op session after a
> boss kill, death, or transition, the message is silently dropped and the
> session continues."

We already have the anchors for exactly this, from our own M2 recon
([[project-ds2-engine]]):
- `Frpg2RequestMessage.RequestDisconnectUser` — string at `140c824fd`.
- The protobuf factory `FUN_1406b77e0` — touches every outbound message.
- The login/session chokepoint `LoginTaskForNP` and `SteamSessionLight`.

So the first, foundational step is implementable from our own RE + the public
description — no need to copy anyone's mod.

**Note:** LukeYui is also building DS2 seamless (announced, prototype, not yet
released). Per project scope ([[project-ds2sc-scope]]) we keep building and
"compare and optimise" if/when his ships.

## Phasing (each phase needs 2 players to verify)

### M5b.0 — Block desummon (foundational)
Hook the outbound protobuf path; when the message is a disconnect/leave for a
co-op session, drop it. Result: a summoned phantom is **not sent home** on boss
death / host death.
- Chokepoint: wrap the serializer/sender around `FUN_1406b77e0`, or the specific
  send that carries `RequestDisconnectUser`. Identify by decompiling the callers
  of the `RequestDisconnectUser` string ref (`140c824fd`).
- **First verifiable sub-goal:** summon a friend, kill a boss (e.g. Last Giant) —
  the phantom stays instead of returning home.

### M5b.1 — Follow on warp / area transition
Blocking disconnect keeps the session, but a phantom must also **load into** the
host's new area on bonfire warp / fog transition. Needs the phantom to re-enter
the host's map rather than idle in the old one. Likely re-summon-on-transition
driven by the coordinator (ds3os) + a session-follow hook.

### M5b.2 — Remove multiplayer boundaries / fog walls
DS2 blocks phantoms at certain fog walls and multiplayer area boundaries. Remove
those gates so partners traverse everything together.

### M5b.3 — Progress / world-state sync (hardest)
Shared boss kills, events, and progression so worlds stay consistent. Optional /
toggleable (Luke's exposes a "full progress sync" toggle).

## M3 hookup (lands with M5b once sessions persist)
With partners persistently in-world, read the live phantom count and call
`player_count::set(N)` so HP scaling reflects the real co-op size — replacing
`_debug_player_count`.

## Clean-room question (needs user ruling)

`scheissgeist/Seamless` (https://github.com/scheissgeist/Seamless) is an
**open-source** community DS2 seamless mod. Our [[feedback-cleanroom-rule]]
forbids disassembling **LukeYui's** DLLs; it doesn't explicitly cover a
*different author's open-source* seamless mod. Two stances:
- (a) Treat it like ds3os / the cheat table — open-source community work we may
  *read for reference* (offsets, message IDs) while writing our own code.
- (b) Treat any *seamless co-op mod* (the thing we're building) as off-limits in
  spirit, and implement only from the public technique + our own recon.

**Deferred to the user.** Until ruled, M5b.0 proceeds from our own recon + the
public description only (which is sufficient for the disconnect-block step).

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
