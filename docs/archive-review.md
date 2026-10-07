# Supplied archive review

Reviewed 2026-10-07 using ZIP metadata, SHA-256 hashing, and the two plaintext configuration/localization files only. No executables were run, loaded, disassembled, or reverse engineered. This follows the closed-binary boundary in [M5b-design.md](M5b-design.md#clean-room-ruling-2026-05-27). Hashing opaque binary bytes establishes identity, not behavior or safety.

## Artifact and inventory

User-supplied filename: `Dark Souls II SoTFS - Seamless Co-op v0.0.5 1468 0.0.5 2026-10-04T23-30Z nUt1ZkmJ.zip`.

Archive size: **15,754,576 bytes**. Archive SHA-256:
`4e16ee411ca18e876e4d609ecb1d7ca60c433741272f2956d0a530ccac129732`.

All eight entries are listed below in archive order: five files and three explicit directories. Sizes are bytes; file hashes cover complete uncompressed contents. Directory hashes cover the empty payload, not their metadata or descendants.

| Entry | Uncompressed bytes | Compressed bytes | SHA-256 |
|---|---:|---:|---|
| `ds2sc_launcher.exe` | 168,448 | 91,525 | `4fb07cd36e17fba7755597395bc1b44a0809358128e8c8353e8add65583fa88c` |
| `SeamlessCoop/` | 0 | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `SeamlessCoop/crashpad/` | 0 | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `SeamlessCoop/crashpad/crashpad_handler.exe` | 824,832 | 389,278 | `d799b428ecc200a47b08b27f6b33ed5fe1f1e065136f380f6a6e78088c404649` |
| `SeamlessCoop/locale/` | 0 | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `SeamlessCoop/ds2sc_settings.ini` | 2,913 | 1,046 | `d454bcb143274d7bd9aeca3bf39e9e9bdc8319d6b5822db18d725bb67e73f31f` |
| `SeamlessCoop/locale/english.json` | 6,137 | 1,893 | `540df58e565a5dd9f21834ae99584a998204c1908042aec18409f3700b86240c` |
| `SeamlessCoop/ds2sc.dll` | 28,592,144 | 15,269,516 | `5df6c51fae1c0a177090e39831402120a7913fe42c2625da32767de562d17a71` |

Total file payload: **29,594,474 bytes** uncompressed, **15,753,258 bytes** compressed; the archive also contains ZIP headers/metadata. All entries were successfully read with Python's ZIP CRC checking. Listed names have no absolute paths or `..` components. These limited checks do not certify the executables as safe.

## Plaintext findings

The INI parses into five sections (`PASSWORD`, `GAMEPLAY`, `SCALING`, `SAVE`, `LANGUAGE`) with 18 settings. Defaults and comments describe:

- A blank shared session password and blank language override; comments say language defaults to the game's Steam language. Only English localization is included.
- Invaders, hostile voice communications, death debuffs, enhanced-enemy highlighting, controller deadzone adjustment, and borderless fullscreen enabled (`1`). Player-relative audio is disabled (`0`), overhead display is ping (`2`), and startup volume is `50`.
- Per-additional-player enemy health/damage/posture scaling of `80`/`15`/`0`, and boss scaling of `100`/`25`/`0` percent, as described by comments.
- A separate save extension, `co2`. There are no coordinator host, port, or public-key settings in this archive INI.

The English JSON parses successfully and has **83 unique entries**. Its text describes hosting, joining, leaving, invasions, world-rule changes (PvP and friendly fire), covenant-themed matchmaking, player death/warp notifications, and curse-related mechanics. Two description entries contain explicit placeholder text. Its `CREDITS` field reads `Yui`; that is an attribution clue inside the artifact, not independent verification of its author or distributor. Feature labels and settings are evidence of intended interface text only: runtime implementation, compatibility, network behavior, save isolation, and gameplay correctness were not verified.

## Relationship to the repository

This is a binary distribution, not a source import. The archive's launcher, DLL, and settings names coincide with repository build output names, but this does **not** establish common authorship, matching implementations, source availability, or the ability to rebuild these supplied binaries. The filename's `v0.0.5` label is not a verified version for repository-built artifacts.

Evidence in the existing source and documentation distinguishes this project:

- [Repository settings](../dist/SeamlessCoop/ds2sc_settings.ini) default to invaders off (`0`), overhead display `0`, enemy health scaling `35`, and enemy/boss damage scaling `0`; they add `skip_intros`, `_debug_player_count`, and coordinator connection settings. They share `co2` and some setting names with the ZIP but are not its INI. The source parser accepts posture keys even though the repository's shipped template omits them.
- [Bootstrap code](../dll/core.cpp) installs version-gated DNS, save-file, and coordinator redirection hooks, initializes HP scaling, and obtains player count from the debug setting. Merely parsing a setting does not mean its advertised feature is implemented.
- [M5b design](M5b-design.md) explicitly records persistent seamless presence as design/feasibility work with multiplayer validation required. Archive locale descriptions do not change that project status.
- [DLL](../dll/CMakeLists.txt) and [launcher](../launcher/CMakeLists.txt) targets build the repository's own C++ sources. They do not build the supplied Crashpad executable or English locale. The filename `crashpad_handler.exe` suggests a crash-reporting component; its origin, version, configuration, and actual behavior were not investigated.

Do not replace the repository settings with the archive settings or present this source tree as the source release for the ZIP. Repo-built outputs and supplied artifacts must remain separately identified.

## Provenance and redistribution limits

No source code, build recipe, README, license, redistribution permission, or third-party notices are present in the complete archive inventory. The supplied filename and ZIP timestamps are packaging metadata, not authenticated release history. No original download URL, signed checksum, signature verification, or independently authenticated publisher was supplied for this review. SHA-256 values identify this exact upload; they cannot authenticate it without a trusted reference.

The locale credit alone cannot establish ownership or grant permission. A component's apparent name (including Crashpad) does not establish which upstream license applies to the shipped bytes or whether required notices were supplied. Missing notices do not prove infringement, but redistribution rights for the archive, its binaries, and its locale remain **unestablished**. Do not infer that any repository license covers them. A source license audit is separate from this archive review.

The ZIP, its binaries, and full locale text are not added to this repository. Publishing or bundling them would require verified provenance and appropriate permission/license evidence. This report records factual metadata and summarizes configuration/interface observations without reproducing locale prose wholesale.

## Reproduce the inventory

Requires Python 3's standard library and access to the original ZIP. Set `ARCHIVE` to its local path; this reads and hashes contents in memory without extracting or executing files. The output includes the archive hash/size and every entry's uncompressed size, compressed size, and SHA-256 in archive order. Reading each member also checks its CRC.

```sh
export ARCHIVE='/path/to/the/supplied.zip'
python3 - <<'PYCODE'
import hashlib
import os
import zipfile
from pathlib import Path

archive = Path(os.environ["ARCHIVE"])

def digest(stream):
    h = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        h.update(block)
    return h.hexdigest()

with archive.open("rb") as stream:
    print("archive", archive.stat().st_size, digest(stream))
with zipfile.ZipFile(archive) as z:
    for entry in z.infolist():
        with z.open(entry) as stream:
            checksum = digest(stream)
        print(entry.filename, entry.file_size, entry.compress_size, checksum)
PYCODE
```
