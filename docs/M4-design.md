# M4 — Save-file redirect (.sl2 → .co2)

> Status: **implemented + verified on hardware.**

## Goal

Co-op play must never touch the player's vanilla save. Redirect all save I/O to
a mod-specific extension (`save_file_extension`, default `co2`) so the vanilla
`DS2SOFS0000.sl2` is left completely untouched as a backup.

Decision (confirmed with user): **fresh co-op character** — pure redirect, no
seeding. The mod opens `.co2`; if none exists the game creates a fresh save
container there. The vanilla `.sl2` is never opened by the hooked game.

## Approach: IAT-hook CreateFileW + CreateFileA

DS2 builds the save path with the literal `.sl2` (string at `1411b5638`, single
xref `0x140a8739e`) then opens it via `CreateFileW`. Rather than patch game code,
we IAT-hook the file-open API (reusing the M2 `patch_iat` helper) and rewrite any
path containing `.sl2` to the configured extension before passing it to the
original. Both W and A are hooked (W is what fires in practice).

```
KERNEL32.DLL!CreateFileW  IAT slot 141aae134
KERNEL32.DLL!CreateFileA  IAT slot 141aae184
```

Matching is a case-insensitive substring test for `.sl2`; rewrite replaces every
`.sl2` occurrence with `.<ext>` (also handles a hypothetical `X.sl2.bak`). The
redirect runs on every open; only the first is logged (DS2 opens the save
container ~17× per save cycle).

## Implementation

- `dll/hooks/savefile_hook.{h,cpp}` — `configure(ext)` + `install()`.
- `dll/core.cpp` — after the DNS hooks: `savefile::configure(s.save_file_extension);
  savefile::install();`
- Reuses `dll/hooks/iat.cpp` (`patch_iat`).

## Verification result (2026-05-27)

Save folder before launch:
```
DS2SOFS0000.sl2   8,251,680   12:00:06   (Majula character)
```
After launching the mod and creating a character:
```
DS2SOFS0000.sl2   8,251,680   12:00:06   (UNCHANGED — vanilla save untouched)
DS2SOFS0000.co2   8,251,680   12:01:49   (fresh mod save container)
```
Log:
```
savefile: hooks installed (.sl2 -> .co2)
savefile: redirect DS2SOFS0000.sl2 -> DS2SOFS0000.co2
```
The game wrote only to `.co2`; the vanilla `.sl2` was never opened for writing.
Confirmed end-to-end: vanilla save preserved, mod uses a separate save.

## Notes / future

- To play an existing vanilla character in co-op, copy `DS2SOFS0000.sl2` →
  `DS2SOFS0000.co2` once before launching (manual, or a future opt-in seed).
- Steam Cloud may still sync the `.sl2`; the `.co2` is local-only unless added to
  the cloud allowlist. Fine for a personal mod.
