# ds2sc — experimental DS2 SOTFS co-op

A C++20, Windows x64 launcher and injected DLL for **Dark Souls II: Scholar of the First Sin** (Steam app ID `335300`). This repository is an experimental implementation with private-coordinator networking groundwork; **persistent seamless co-op is unfinished**.

The separately supplied “Seamless Co-op v0.0.5” ZIP contains compiled software, settings, and localization, not this project's source. A shared filename or settings format does not establish that these sources produced that release. See the [archive review](docs/archive-review.md) for inventory, provenance limits, and redistribution considerations. Repository builds produce only the implementation here; the supplied binaries are not included.

**No license is currently supplied for this repository.** Do not infer reuse or redistribution permission from the presence of source or from licenses of referenced projects.

## Implementation status

- Launcher starts `DarkSoulsII.exe` suspended, injects `SeamlessCoop/ds2sc.dll`, and resumes it.
- DLL checks the executable's SHA-256 before installing hooks. The expected hash is `0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131`, identified in the code as SOTFS v1.0.3.0. Other versions are refused; this is not a general compatibility guarantee.
- Hooks block selected FromSoftware DNS lookups, redirect `.sl2` save opens to `.co2` by default, and apply separate enemy/boss HP scaling using a debug player count.
- Optional hostname, RSA public-key, and login-port redirection targets a separately operated [ds3os](https://github.com/TLeonardUK/ds3os) coordinator in `DarkSouls2` mode. Historical milestone notes record a local server handshake; a real two-player summon test remains outstanding.

Still unfinished: persistent partners after boss/death events, phantom host permissions, warp/area following, shared progression/world-state synchronization, live session player count, and damage scaling. Gameplay toggles (invasions, death debuffs, nametags, intro skipping), password matching, and language override are parsed but not wired to gameplay/localization behavior. Setting them does not implement those features.

## Prerequisites and build

Use Windows x64 with Visual Studio 2022 or Build Tools 2022, the **Desktop development with C++** workload (MSVC and Windows SDK), CMake 3.20+, and PowerShell for the helper script. Building requires no game files. Running requires your own DS2 SOTFS installation and Steam; coordinator testing additionally requires a separate ds3os server and its public key.

From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or use `.\scripts\build.ps1` (defaults to Release), with `-Config Debug` or `-Clean` as needed. The default build stages a self-contained package under `build/package/Release/`:

```text
ds2sc_launcher.exe
SeamlessCoop/
  ds2sc.dll
  ds2sc_settings.ini
```

A normal build does not write into a game installation. To explicitly stage into your chosen **Game** directory containing `DarkSoulsII.exe`:

```powershell
.\scripts\build.ps1 -StageToGame -GameDir 'D:\SteamLibrary\steamapps\common\Dark Souls II Scholar of the First Sin\Game'
```

The equivalent CMake opt-in is:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DDS2SC_STAGE_TO_GAME=ON -DDS2SC_GAME_DIR="D:/SteamLibrary/steamapps/common/Dark Souls II Scholar of the First Sin/Game"
cmake --build build --config Release
```

Replace the example path with your own. Staging copies the launcher, DLL, and default INI, so preserve personal settings before repeating it. For raw CMake builds, the opt-in persists in the build cache until reconfigured with `-DDS2SC_STAGE_TO_GAME=OFF`; the helper resets it unless `-StageToGame` is specified. The Windows workflow builds and checks the package; a CI build alone does not verify gameplay.

## Settings and running

The launcher must sit beside `Game/DarkSoulsII.exe`, with the DLL and settings beneath `Game/SeamlessCoop/`. Start `ds2sc_launcher.exe` from that layout. Back up saves before experimental runtime testing. Check `SeamlessCoop/ds2sc.log` for the hash-gate result and installed hooks; a hash mismatch leaves the game running without the mod's hooks, including save redirection and DNS blocking.

Edit the installed copy of [ds2sc_settings.ini](dist/SeamlessCoop/ds2sc_settings.ini), keeping the repository copy as a non-personal default:

| Setting | Current behavior |
| --- | --- |
| `save_file_extension = co2` | Redirects save file opens; normally creates a fresh mod save rather than copying a vanilla character. |
| `enemy_health_scaling`, `boss_health_scaling` | Percentage added per extra player: `1 + (N - 1) * percent / 100`. |
| `_debug_player_count = 1` | Supplies `N`; it is not a live co-op count. Default means no HP scaling. |
| `coordinator_host` | Blank leaves redirection disabled, with the DNS block active after a successful version gate. |
| `coordinator_pubkey_file` | RSA **public** key PEM next to the DLL/settings; default `coordinator_pubkey.pem`. Never share the coordinator's private key. |
| `coordinator_port = 50050` | Rewrites the game's login connection port `50031` for the coordinator. |

For coordinator experiments, all clients need the same reachable coordinator host and its public key. The redirect enforces hostname/key buffer limits. Use an LF-normalized RSA public-key PEM (the current hook does not normalize CRLF). Follow [M5 networking notes](docs/M5-design.md) for historical setup details; the coordinator is not bundled or started by this project. `cooppassword` currently has no matchmaking effect in this DLL.

## Repository map and design notes

| Path | Purpose |
| --- | --- |
| `dll/` | Bootstrap, settings/logging, version gate, IAT hooks, and scaling. |
| `launcher/` | Windows game launcher and DLL injection. |
| `dist/SeamlessCoop/` | Tracked default settings, not a binary release. |
| `scripts/` | Build helper and optional Ghidra analysis helper. |
| `ghidra-scripts/`, `recon/` | Game-analysis scripts and recorded findings; not required to build. |
| `docs/` | Milestone plans, research, and archive review. |

Read [M2 plan](docs/M2-plan.md) and [M2 design](docs/M2-design.md) for version gating/network isolation, [M3](docs/M3-design.md) for HP scaling, [M4](docs/M4-design.md) for save redirection, [M5](docs/M5-design.md) for private networking, and [M5b](docs/M5b-design.md) for unfinished seamless presence. These are historical research documents: some status headings predate later code, and `[[...]]` references point to external project notes rather than files in this repository. Runtime behavior is defined by the current code.

See [CONTRIBUTING.md](CONTRIBUTING.md) for changes, validation, and reporting issues.
