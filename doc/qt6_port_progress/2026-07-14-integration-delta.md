# Qt 6 Migration Integration Delta — 2026-07-14

This report is the current delta to
`2026-07-13-branch-audit.md`. It records only refreshed facts and the R0/R1
integration boundary; the July 13 audit remains the detailed source snapshot.

## R0 Refresh

- Starting migration tip: `7d923dbe8e26000b1d184184f3b5e1bbd2837b09`.
- Refreshed `origin/master`: `9b10b9aea0e2309a3597e4f2270da3ad88dc0b5b`.
- Pre-integration merge base: `6fb1268177b9cc06baf3670229bdf027e8ebe68a`.
- Pre-integration comparison: migration branch 113 commits ahead and 12
  commits behind refreshed `origin/master`.
- Tracked worktree state: clean before the refresh and merge.
- Refreshed GitHub state on July 14, 2026: no pull request exists for
  `codex/qt-6-port`; the newest Qt 6 experimental workflow is still the
  [failed July 13 run](https://github.com/bgyss/opentoonz/actions/runs/29245050730)
  at `6fb126817`, with macOS arm64 successful, Windows failing during CMake
  configuration, Linux failing during compilation, and publication skipped.
  No `qt-6-experimental` release was listed.
- Local toolchains resolved by `mise run doctor` and `mise run doctor-qt6`:
  Qt 5.15.18 and Qt 6.11.0 respectively, with CMake 4.1.2 and Ninja 1.13.2.

The CI observation did not materially change from the July 13 audit. The live
master tip and local branch tip were refreshed and therefore supersede the
corresponding values in that audit.

## R1 Integration Boundary

Live master was integrated with the merge-only commit
`991804f3e` (`Merge master into Qt 6 port`). No shader, scripting, packaging,
or compatibility cleanup was included in that commit.

The eight overlapping paths were reviewed explicitly:

| Path | Decision |
|---|---|
| `toonz/sources/tnztools/CMakeLists.txt` | Intentional combination: retain the Qt-major-aware target changes and add master's `editassistantstool.h` source registration. |
| `toonz/sources/toonz/commandbarpopup.cpp` | One additive include conflict was resolved by retaining both `qtcompat.h` and master's `menubarcommand.h`; master's hidden/duplicate Assistant command filtering and the branch's Qt-compatible event handling were both retained. |
| `toonz/sources/toonz/levelcreatepopup.cpp` | Intentional combination: retain the branch's Qt-compatible code and master's Assistant-level creation/default-assistant behavior. |
| `toonz/sources/toonz/mainwindow.cpp` | Intentional combination: retain Qt 6 action/event compatibility and add master's Assistant-level actions and tool-option commands. |
| `toonz/sources/toonz/menubar.cpp` | Intentional combination: retain Qt 6 menu behavior and expose master's New Assistant Level entry. |
| `toonz/sources/toonz/shortcutpopup.cpp` | Intentional combination: retain Qt-compatible shortcut handling and add master's filtering of invisible/duplicate Assistant commands. |
| `toonz/sources/toonz/tapp.cpp` | Intentional combination: retain the branch's application compatibility changes and master's Assistant-level tool auto-switch/restore behavior. |
| `toonz/sources/toonzlib/preferences.cpp` | Intentional combination: retain Qt-major-aware preference behavior and add master's `DefAssistantType` preference. |

The large migration delta should be reviewed by the stable requirement and
subsystem boundaries in `doc/qt6_migration_goal_prompt.md`: integration and
support policy, build/API compatibility, scripting, rendering, multimedia and
input, packaging/runtime data, and QA/evidence. This is the accepted local
review map until a draft pull request exists.

## Integration Validation

All results below are from the integrated `991804f3e` source tree on macOS
arm64. Nix cache state was redirected to a writable temporary path for the
Codex sandbox; this changed no tracked source or build configuration.

| Requirement | Command | Result | Scope |
|---|---|---|---|
| `QT6-INT-01`, `QT6-API-01` | `bash scripts/qt6/check-migration-preflight.sh` | Pass | All registered mechanical migration guards, including 9 shader interfaces and 16 program references. |
| `QT6-INT-01`, `QT6-BLD-01` | `XDG_CACHE_HOME=/tmp/opentoonz-codex-cache mise run configure` | Pass with Qt 5.15.18 | Qt 5 configuration only; not a build or runtime result. |
| `QT6-INT-01`, `QT6-BLD-01` | `XDG_CACHE_HOME=/tmp/opentoonz-codex-cache mise run configure-qt6` | Pass with Qt 6.11.0 | Qt 6 configuration only; not a build, minimum-version, or runtime result. |

`QT6-INT-01` has passed its local integration/preflight/configure gate. It is
not release-complete: a draft PR and cross-platform same-commit build evidence
remain outstanding. R2 support-policy work and R3 builds must use descendants
of `991804f3e`.

## R2-R6 implementation delta

The support contract and build frontier are now implemented on descendants of
the integration merge:

- bf7e97b11 records the Qt 6.9 API floor, Qt 6.10.3 release pin, Qt 6.11
  forward lane, strict deprecation threshold, platform matrix, parity baseline,
  and named corpus.
- 118c3046f fixes the translation output root and removes the tracked generated
  toonz/stuff/config/loc tree; the translation lane now produces 63 QMs under
  the packaged root.
- a258f4f2e adds controlled Qt 6 OpenGL context and framebuffer diagnostics.
  The interactive shader pixel smoke remains headless-blocked on this host.
- 75500a1d3 moves Qt 6 evaluation through the existing executor thread;
  9b4c1991e marshals Script Console viewer operations to the GUI thread.
  Persistent QJSEngine ownership remains an explicit follow-up, documented in
  2026-07-14-script-runtime-boundary.md.
- bc518f17e rejects unsupported Qt 6 audio format substitutions instead of
  reinterpreting unchanged PCM bytes.
- 839ed9dc0 and acb7c27c8 enable translation/package runtime checks,
  checksums/source-commit recording, and stable manual-goal projections.

Fresh same-commit local evidence is retained under
toonz/build/qt6-evidence/: Qt 5, Qt 6, strict Qt 6.10 deprecation, and
translation-enabled builds passed with Qt 5.15.18 / Qt 6.11.0. The incremental
Qt 6 script/audio/viewer build also passed. This is macOS arm64 evidence only;
Linux/Windows packages, clean packaged launch, real shader pixels, interactive
script cancellation, hardware input/audio/camera, and signoff remain external
gates.
