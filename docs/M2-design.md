# M2 — Online disconnect + version gate (DESIGN)

> Status: **design complete, ready for implementation**.
> Based on Ghidra recon output in `recon/`. All addresses are valid for SHA-256 `0045931B…73131` (SOTFS v1.0.3.0).

## Goal

When `ds2sc_launcher.exe` starts the game:

1. **Refuse to install hooks** if the host exe SHA-256 doesn't match the expected version.
2. **Prevent FromSoft's matchmaking servers** from being contacted (`*.fromsoftware-game.net`).
3. **Leave Steamworks fully alive** — friends list, Steam P2P, identity, presence. We need these for M5+ coop.
4. **No game crash on title screen.** Online options can grey out or show errors; just don't freeze.

## Why DNS-level instead of game-internal hooks

After examining four candidate chokepoints in Ghidra (`recon/chokepoints.txt`), each in-binary hook carries non-trivial side-effect risk:

| Candidate | Risk |
|---|---|
| `FUN_140289f30` (login URL prep, 5 callers) | Callers may not handle no-op gracefully → potential freeze. Some callers are at unresolved addresses (`<no fn>` — likely vtable slots) which we can't easily audit. |
| `FUN_1406b77e0` (protobuf factory) | Touches **every** outbound message. Drop-listing is fragile; we'd need to keep the deny-list in sync with FromSoft's message catalog. |
| `LoginTaskForNP::*` | CCallback class data at `0x141591ce0` has **no Ghidra-recoverable xrefs** (RTTI-only) — finding the entry method requires manual decompile work. |
| `getaddrinfo` IAT patch | One symbol, one patch. Survives whatever the game does internally. Brittleness moves from "FromSoft's code" (which we don't control) to "FromSoft's hostnames" (which we do — we just hardcode the suffix). |

DNS-level is the lowest-risk path that achieves M2's goal. We can revisit in-binary hooks at M5 when we replace matchmaking with our own, by which point we'll have lived with the binary long enough to understand it.

## Hook plan

### Step 1: SHA-256 version gate

Compute SHA-256 of the on-disk `DarkSoulsII.exe` (path from `GetModuleFileNameW(NULL)`) using Win32 BCrypt. Compare to:

```cpp
constexpr uint8_t EXPECTED_SHA256[32] = {
    0x00,0x45,0x93,0x1B, 0x89,0x14,0x50,0x45,
    0x31,0xB7,0x86,0x4A, 0x94,0x88,0xD3,0x96,
    0xDC,0x50,0xCB,0xAF, 0x52,0x49,0x64,0x01,
    0x6E,0x1D,0x69,0xC3, 0xD1,0x17,0x31,0x31,
};
```

On mismatch: log `"refusing to install hooks — host exe is not SOTFS v1.0.3.0"` and return. Game runs vanilla. No crash, no corruption.

### Step 2: `getaddrinfo` IAT patch

Single IAT slot at `0x141aae64c` (image-relative `0x1aae64c`). Patch sequence:

1. Walk PE import directory of the host module to find the ws2_32!getaddrinfo IAT slot. Don't hardcode `0x141aae64c` — compute from the live module so it survives ASLR.
2. Save original `getaddrinfo` pointer.
3. `VirtualProtect` the IAT page to PAGE_READWRITE, overwrite the slot with our hook, `VirtualProtect` back to PAGE_READ.

Our hook (Windows calling convention `__stdcall`, matches `ws2_32!getaddrinfo`):

```cpp
int WSAAPI hooked_getaddrinfo(
    PCSTR pNodeName, PCSTR pServiceName,
    const ADDRINFOA* pHints, PADDRINFOA* ppResult)
{
    if (pNodeName && is_fromsoft_hostname(pNodeName)) {
        log::info("getaddrinfo: blocked " + std::string(pNodeName));
        WSASetLastError(WSAHOST_NOT_FOUND);
        return WSAHOST_NOT_FOUND;
    }
    return original_getaddrinfo(pNodeName, pServiceName, pHints, ppResult);
}

bool is_fromsoft_hostname(const char* host) {
    // case-insensitive suffix match
    static const char* deny[] = {
        ".fromsoftware-game.net",
        ".fromsoftware.jp",
    };
    size_t n = std::strlen(host);
    for (auto* s : deny) {
        size_t m = std::strlen(s);
        if (n >= m && _stricmp(host + (n - m), s) == 0) return true;
    }
    return false;
}
```

### Step 3: `gethostbyname` IAT patch (defense in depth)

Same pattern at `0x141aae5dc`. Legacy DNS API — DS2 may not actually use it, but cost-free to also patch.

### Step 4: `connect` IAT patch (deferred unless needed)

`0x141aae624`. Only add if Steps 2-3 prove insufficient (e.g. if DS2 caches resolved IPs and reconnects directly). For now: skip.

## What we do NOT do at M2

| Skipped | Why |
|---|---|
| Disable SteamMatchmaking | Steam lobbies are how we'll do M5 coop discovery; leave alive. |
| Hook protobuf factory | Too broad. Belongs to M5+ when we route messages to our own backend. |
| NOP `LoginTaskForNP` | We don't know the entry point cleanly. DNS block achieves the same outcome. |
| Save file split | M4 work. The `.sl2` xref at `0x140a8739e` (in `FUN_140a87110` `SaveLoad2::SLContentFormat` ctor, line 189 of `chokepoints.txt`) is logged for that milestone. |
| Param/scaling | M3 work. |

## Implementation files (proposed)

```
dll/
  hooks/
    iat.h, iat.cpp           # generic "patch this import" helper (PE walk + VirtualProtect)
    getaddrinfo_hook.cpp     # the actual hook function
  version_gate.h, .cpp       # SHA-256 of host exe via BCrypt
  core.cpp                   # bootstrap: gate → install hooks
```

No third-party deps. Pure Win32 + `<bcrypt.h>`. Adds two link libs: `bcrypt.lib`, `ws2_32.lib`.

## Acceptance criteria

- [x] Smoke test: `ds2sc_launcher.exe` brings up DS2 title screen, no crash. *(Already passing from M1.)*
- [ ] `ds2sc.log` shows `version_gate: SHA-256 OK` and `hooks: getaddrinfo IAT patched at <addr>`.
- [ ] `ds2sc.log` shows `getaddrinfo: blocked frpg2-steam64-ope-login.fromsoftware-game.net` shortly after title screen appears.
- [ ] Title screen does NOT show "Searching for messages" / no online banner.
- [ ] If we try to open the Online menu, it errors out gracefully (not a freeze).
- [ ] Closing the game produces a clean teardown log line.

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| Game checks resolved IP on a fast path and crashes when getaddrinfo fails | Try returning a localhost IP instead of WSAHOST_NOT_FOUND. The subsequent `connect()` to 127.0.0.1:443 will refuse cleanly, which is a softer failure. |
| FromSoft uses additional hostnames we haven't catalogued | Log all blocked + passed lookups for a few sessions, expand `deny[]` as needed. |
| Steam's own networking (P2P) is somehow affected | Verified offline: Steam P2P uses internal IPC + relay, not ws2_32 directly. Even so, double-check by monitoring `is_fromsoft_hostname` log output for any unexpected Steam lookups. |
| Future game patch shifts the IAT | We compute IAT slot from live module each boot, not hardcoded. Survives. SHA-256 gate prevents installing on a different binary. |

## After M2

With FromSoft offline successfully muted, M3 (regulation params for per-player scaling) and M4 (save file split to `.co2`) become independent and can land in either order.
