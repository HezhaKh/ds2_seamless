# Contributing

Read the [README](README.md), relevant [milestone docs](docs/), and [archive review](docs/archive-review.md) before proposing a change. This is experimental Windows x64 code; distinguish implemented behavior, historical test results, and planned features. No license is currently supplied, so clarify permission before copying or redistributing repository material.

## Changes

1. Create a focused branch from the current base and keep changes scoped to one problem.
2. Follow the existing C++20 style: four-space indentation, `ds2sc` namespaces, small helpers, and explicit Win32 error handling. Keep build paths configurable and game-directory writes opt-in.
3. Preserve the executable hash gate and separate-save defaults. Changes to offsets, memory writes, hooks, or save handling need an explanation of their safety assumptions and failure behavior.
4. Build using the [README commands](README.md#prerequisites-and-build). Review `git diff --check`, `git diff`, and `git status --short` before opening a PR.
5. Explain what changed and why, how it was validated, and what still needs hardware or multiplayer testing. Use the PR template.

## Validation

A Windows x64 Release build is the minimum check for code/build changes; include the compiler, CMake version, configuration, and result. There is currently no automated gameplay test suite. Add focused tests where practical and relevant; documentation-only changes need link, command, and whitespace checks.

Runtime checks require an explicitly chosen game installation and backed-up saves. Confirm the hash-gate and hook results in `SeamlessCoop/ds2sc.log`. Save changes should verify both the vanilla save's preservation and the separate mod save. Networking or seamless changes need an actual two-player test when relevant: report coordinator configuration, reproducible steps, and observed behavior. A successful build or local handshake does not establish multiplayer compatibility. If a test cannot be run, state that directly.

## Reports and repository hygiene

Use the bug or feature template. Include the source commit/build and say whether the issue concerns a repository build or the separately supplied binary release. Describe expected and actual behavior with minimal reproduction steps. Redact personal paths, addresses, passwords, Steam identifiers, and sensitive log content; share relevant text excerpts rather than entire private environments.

Do not commit game executables/assets, saves, generated EXE/DLL/PDB files, release packages, supplied closed binaries, coordinator private keys, or personal settings. The tracked `dist/SeamlessCoop/ds2sc_settings.ini` is a public default and must remain tracked. Ignore rules are only a convenience: inspect staged files, including keys with custom names, before submitting.

The existing [M5b scope ruling](docs/M5b-design.md#clean-room-ruling-2026-05-27) permits research from documented open-source/base-game references while excluding closed seamless DLL reverse engineering. Cite sources and their applicable permissions for new research or dependencies; do not assume another project's license covers this repository. Keep supplied binary archive review static and separate from source implementation claims.
