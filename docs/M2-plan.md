# M2 — Online disconnect + version gate

> Status: **not started**. Blocked on Ghidra install + initial DS2 RE pass.
>
> All addresses below are **placeholders**. They will be filled in only after Ghidra static analysis confirms them on `DarkSoulsII.exe` SHA-256 `0045931B…73131`.

## Goal

When the game starts under `ds2sc_launcher.exe`:

1. Refuse to run if the host exe is not the expected SOTFS v1.0.3.0 build (SHA-256 hash gate, computed once at injection).
2. Prevent the game from contacting FromSoft / Steamworks matchmaking servers (the "Connecting…" / "Searching for messages" online steps that block the title-screen flow).
3. Leave the local Steamworks API alive (we still need Steam's identity, friends list, and P2P sockets for our own networking later).

## Why this comes before everything else

- We can't safely test scaling/save/sync changes if the unmodded online layer is still phoning home and potentially soul-memory-reporting or invasion-matching.
- It's the smallest hook surface that produces an observable effect (title screen no longer says "Searching for messages").
- It's reusable: every subsequent milestone runs in this offline-but-Steam-alive sandbox.

## What we need from Ghidra

Run these analyses on `DarkSoulsII.exe` once the install is up:

1. **String search** for `"connecting"`, `"reconnect"`, `"matchmaking"`, `"GfwLive"`, `"GhostStone"`, `"NetMan"`, `"FrpgNet"`, `"BloodStain"`, `"sl2"`. Cross-reference each to find the function that consumes it.
2. **Imports** — list every `steam_api64.dll` import (`SteamAPI_Init`, `SteamMatchmaking*`, `SteamNetworking*`, `SteamGameServer*`, etc.). Note which ones are used in the loader vs. mid-game.
3. **Cross-refs to `steam_api64.dll!SteamAPI_Init`** — find the init function and trace up to identify the global Steamworks context the game uses.
4. **Search for typical FrpgNet symbols.** From community DS3/ER work the FROMSoft network layer historically uses names like `FrpgNetMan`, `FrpgNetCommSession`, `FrpgNetMsg`. DS2 may differ but the pattern of a single "NetMan" singleton with a polling loop is consistent across their engine.
5. **Identify the title-screen "press start" → "online check" transition.** From DS3 community work this is usually a state machine in `MenuMan` / `TitleScreen` that calls into `NetMan->begin_online`. We need the entry point so we can early-return.

## Candidate hook strategies (pick after Ghidra recon)

| Strategy | Pros | Cons |
|---|---|---|
| Hook `SteamMatchmaking_*` exported from `steam_api64.dll` via IAT patch | Easy to find (PE import table). Survives game updates. | Only blocks Steam-side matchmaking; doesn't stop FromSoft's own server pings. |
| Hook `FrpgNetMan::begin_online` (or DS2 equivalent) and return immediately | Single chokepoint, very clean. | Requires we find the actual function — bytes-level work in Ghidra. |
| Set a runtime "offline" global (if one exists) | Cleanest possible. Mirrors how the game itself flips offline when Steam is down. | Existence not confirmed — needs Ghidra. |
| Block the network thread by hook'ing `WSAStartup` / `WSAConnect` | Brutal, undeniable. | Will also brick our own future net code. Don't. |

**Most likely plan:** combination of (1) IAT patch for `SteamMatchmaking_*` (defence in depth) + (2) `FrpgNetMan` chokepoint once identified.

## Version gate design

```cpp
// In core.cpp::bootstrap, before any hook installation:
constexpr std::array<uint8_t, 32> EXPECTED_SHA256 = {
    0x00,0x45,0x93,0x1B,0x89,0x14,0x50,0x45,
    0x31,0xB7,0x86,0x4A,0x94,0x88,0xD3,0x96,
    0xDC,0x50,0xCB,0xAF,0x52,0x49,0x64,0x01,
    0x6E,0x1D,0x69,0xC3,0xD1,0x17,0x31,0x31,
};

if (sha256_of_host_image() != EXPECTED_SHA256) {
    log::error("host exe does not match SOTFS v1.0.3.0 — refusing to install hooks");
    return /* fail-safe: do nothing, game proceeds vanilla */;
}
```

We hash the on-disk file (via the path from `GetModuleFileNameW(nullptr)`), not the in-memory image — relocations and section permissions perturb the in-memory bytes.

## What we do NOT do at M2

- Save file split (M4).
- Param scaling (M3).
- Any networking (M5+).
- Disabling the in-memory ASLR check / soul memory / anti-cheat — only what's strictly needed to keep the game running without phoning home. We'll evaluate per chokepoint whether it's necessary.

## Acceptance for M2

- Launching via `ds2sc_launcher.exe` brings up the title screen as normal.
- `ds2sc.log` contains: `host: ...DarkSoulsII.exe`, the version-gate result (`OK SHA256 matched`), and `M2: online disconnect installed`.
- The title screen does NOT show "Searching for messages" or any reconnect banner.
- Pressing Online options either greys them out or shows an immediate error — we don't care which, as long as the game doesn't actually open a session with FROM.
- Closing the game produces no Windows error dialog, no crash in `ds2sc.log`.

Once M2 lands cleanly, M3 (regulation param overrides) and M4 (save file split) can run in parallel because they touch disjoint code.
