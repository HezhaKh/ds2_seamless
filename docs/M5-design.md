# M5 — Co-op networking (DESIGN)

> Status: **design / research pass.** Phased: private coordinator first (native
> co-op), seamless presence later. No code yet.

## Goal

Let the user and friends play DS2 SOTFS co-op together on a private backend,
independent of FromSoft's servers. End goal is *seamless* co-op (persistent
presence); the tractable first step is restored *native* co-op (summon-based)
on a private coordinator.

## Key facts (from recon + community research)

- DS2 session **data already flows peer-to-peer over Steam** (`SteamNetworking`,
  9 call sites). FromSoft's server is only the *coordinator*: login, summon-sign
  registry, matchmaking, message relay (bloodstains/messages). See
  [[project-ds2-engine]].
- The community already reimplemented that coordinator: **`TLeonardUK/ds3os`**
  (MIT; ds2os was merged into it). DS2 mode supports blood messages,
  bloodstains, ghosts, **summoning**, invasions, auto-summon (covenants),
  matchmaking, ticket auth. Experimental but functional.
- **Redirect is plaintext on DS2** (unlike DS3's TEA blob): the login hostname
  and the server RSA public key live as plain strings in the exe. ds3os's
  `DS2_ReplaceServerAddressHook` just AOB-searches and `memcpy`s replacements
  (after `VirtualProtect` to RW). We already know the hostname string address
  from M2 recon: `1411d4ab0` (`frpg2-steam64-ope-login.fromsoftware-game.net`).
- ds3os's Injector mirrors our DLL: it has a server-address hook, a
  **save-filename hook** (= our M4), and a server-port hook. Clean confirmation
  our architecture is right.

**Clean-room:** ds3os is MIT *base-game server emulation* — fine to use and
reference, the same category as the boblord14 cheat table. This is distinct from
LukeYui's seamless-coop DLLs, which remain off-limits ([[feedback-cleanroom-rule]]).

## Phasing

### M5a — Private coordinator + native co-op (tractable; build first)

**Coordinator (no code from us):** run ds3os `Server.exe` with
`GameType: DarkSouls2` (host port-forwards TCP+UDP `50000, 50010, 50050, 50020`,
or LAN/localhost for first tests). It generates a keypair + `config.json`.

**Client redirect (our DLL):** add a hook that, on bootstrap, overwrites two
plaintext strings in the exe with our coordinator's values:
1. login hostname string (`…fromsoftware-game.net`, at `1411d4ab0`) → our
   coordinator host (domain or IP, **must fit the original buffer length** — use
   a short host or patch the length; verify against ds3os approach),
2. the `-----BEGIN RSA PUBLIC KEY-----…` PEM string → our coordinator's public
   key (same PEM format).
Reuse our `VirtualProtect`+write helpers (from `scaling.cpp` / `iat.cpp`).
Reference: ds3os `Source/Injector/Hooks/DarkSouls2/DS2_ReplaceServerAddressHook.cpp`
and `ReplaceServerPortHook.cpp` (MIT). The coordinator host + public key come
from new `[NETWORK]` settings in `ds2sc_settings.ini` (shared among friends).

This **supersedes M2's DNS block**: instead of blocking `*.fromsoftware-game.net`,
we point the game at our coordinator. (Keep the M2 block as defense-in-depth; it
won't match the new hostname.)

**Result:** friends running our launcher, pointed at the same coordinator, get
native DS2 co-op — summon each other, co-op through areas via summon signs.

**First verifiable sub-goal (M5a.0):**
Stand up a local ds3os DS2 server (`GameType=DarkSouls2`, listening on
localhost). Patch the hostname string to the loopback host + the RSA key to the
local server's key. Launch the game; **ds3os logs the client connecting and the
game reaches the online state** (online menus work, no login error). That proves
the redirect + the native protocol handshake end-to-end. Fastest route: first
prove it using **ds3os's own Loader** (confirms the coordinator works at all),
then fold the redirect into our DLL for a single unified launcher.

### M5b — Seamless presence (hard; Luke-level)

Native co-op is summon-based (partners desummon on boss death / some area
transitions). True *seamless* keeps partners persistently in-world. ds3os does
**not** do this. It requires deep in-game session-lifecycle hooking
(anti-desummon, re-summon on transition, fog-gate coordination). This is the
long road and gets its own design pass after M5a is working.

### M5c — World-state sync (hardest)

Sync enemy deaths, events, and progression between partners' worlds so they stay
consistent. Deferred.

## M3 integration

Once in a session, read the live phantom/player count (recon shows
`Frpg2PlayerData.PhantomTypeCount`, `ChrNetworkPhantom*`, `SteamSessionLight`) and
feed `player_count::set(N)`. That replaces `_debug_player_count` so HP scaling
reflects the real co-op size automatically.

## Settings (new, M5a)

```ini
[NETWORK]
; Shared private coordinator (ds3os, GameType=DarkSouls2). All friends use the same.
coordinator_host = 127.0.0.1
; The coordinator's RSA public key (PEM). Friends paste the host's key here.
coordinator_pubkey_file = coordinator_pubkey.pem
```

## Risks

| Risk | Mitigation |
|---|---|
| ds3os DS2 support is experimental | Expect rough edges; verify each co-op feature; report/patch upstream if needed. |
| Replacement hostname longer than original buffer | Use a short host/IP or patch the adjacent length; follow ds3os's exact method. |
| Hosting / NAT (port-forward 50000/50010/50050/50020) | First tests on localhost/LAN; for friends, one host port-forwards or use a small VPS. |
| String locations are version-specific | M2 SHA-256 gate already refuses to run on a non-1.0.3.0 build. |
| Memory patch needs admin (ds3os notes this) | Our launcher injects via CreateProcess; verify privilege needs during M5a.0. |
| Seamless (M5b) is a large RE effort | Phased — M5a delivers playable co-op first; M5b is a separate milestone. |

## Reference sources

- ds3os (MIT): https://github.com/TLeonardUK/ds3os — DS2 coordinator + Injector.
- Tim Leonard, "Reverse Engineering Dark Souls 3 Networking" — connection/crypto
  background (DS2's strings are plaintext, simpler than DS3).
- See [[reference-ds2-cheat-table]] for in-game structure offsets (M5b/M3 use).
