# Qt 6 Port Functional Gaps Report - 2026-07-06

## Snapshot

- Report date: 2026-07-06
- Local `master` baseline before this report commit: `8b108511c`
- Default remote branch observed: `origin/master` at `25677008e`
- Qt 6 branch reviewed: `codex/qt-6-port` at `419ef986f`
- Existing Qt 6 reference docs reviewed from `codex/qt-6-port`:
  - `doc/qt6_migration_study.md`
  - `doc/qt6_migration_goal_prompt.md`
  - `doc/qt6_remaining_work_and_manual_verification.md`
  - `doc/how_to_build_macosx.md`
  - `doc/how_to_build_nix_mise.md`

This report tracks the separate Qt 6 branch and the work still needed before
the port can be treated as release-ready. It does not mean `master` contains
the Qt 6 implementation.

No new user-provided manual verification completions were found in this
thread since the 2026-06-29 report, so checklist status changes below come from
branch, documentation, and CI evidence.

## Current CI And Workflow Evidence

- The Qt 6 branch tip is `419ef986f`. The latest visible regular branch CI
  runs are still for source-changing commit `a115837f5` on 2026-07-01, and
  Linux, macOS, and Windows all succeeded there.
- The two commits after `a115837f5` are documentation and build-evidence
  updates: `3756c93d8` updates deprecated-API build evidence, and `419ef986f`
  clarifies the Qt 5 default build lane versus the dedicated Qt 6 lane.
- The latest observed `Qt 6 Experimental Binary Builds` workflow is the
  2026-06-29 scheduled run on `master`, and it still failed overall.
- The 2026-06-29 experimental package blockers are:
  - Windows x64 failed during CMake configuration because the requested
    `Visual Studio 17 2022` generator could not find a matching Visual Studio
    instance on the scheduled runner.
  - Linux x86_64 AppImage failed while compiling `iwa_bokehreffx.cpp` because
    `UCHAR_MAX` is used without the header that defines it. The current Qt 6
    branch still lacks the `<climits>` include in that file.
- The same 2026-06-29 experimental run produced a successful macOS Apple
  Silicon arm64 build, bundle verification, DMG creation, artifact creation,
  and artifact upload, but the rolling prerelease publish job was skipped
  because the workflow failed overall.
- The Qt 6 branch workflow differs from the scheduled default-branch workflow:
  the branch uses the Windows 2022 runner and the expanded Qt module list,
  while `origin/master` still has the scheduled workflow version that failed.
- The latest observed `Qt 6 Project Sync` workflow on 2026-07-06 completed as
  a workflow but skipped Project mutation because `QT6_PROJECT_TOKEN` is still
  unset.
- Local build, package, GUI, hardware, and studio validation were not rerun
  during this documentation pass.

## Progress Since The Previous Report

- The Qt 6 branch advanced from `c4d5daf48` to `419ef986f`.
- Regular Linux, macOS, and Windows branch CI stayed green through the latest
  source-changing commit, `a115837f5`.
- QJSEngine script compatibility coverage broadened substantially:
  - A user-style workflow fixture now creates a two-frame scene, vectorizes a
    raster level, rasterizes the result back to full-color raster frames,
    inserts the output into a scene-owned level, saves and reopens the scene,
    renders through `Renderer`, reloads rendered output, and disposes public
    wrappers.
  - Scene I/O edge coverage now checks `Scene.save()` / `Scene.load()`
    chain-return behavior for `FilePath` and string paths, with cell contents
    surviving reloads.
  - Level path loading now preserves explicit filesystem-style parent paths
    before scene project decoding and falls back to frame-reader loading for
    script-saved raster sequences when the native scene loader cannot load
    them directly.
  - Level transformer coverage now includes repeated returned wrapper ids and
    independent returned-level lifetime for level-wide vectorization and
    rasterization.
  - ImageBuilder, ToonzRasterConverter, vectorizer, Rasterizer, and Renderer
    edge fixtures now cover more arity, lifecycle, wrapper lifetime, numeric
    validation, and legacy coercion behavior.
- The forced-offscreen Rasterizer gap has narrowed. Under
  `QT_QPA_PLATFORM=offscreen`, the Qt 6 `Rasterizer` binding now falls back
  from `TOfflineGL` to `TRasterImageUtils::vectorToFullColorImage()` so the
  focused offscreen smoke can validate full-color `Image` and `Level` output.
- The forced-offscreen Renderer gap has also narrowed. For simple
  script-created raster/vector cells, the Qt 6 `Renderer` binding now uses a
  camera-sized raster compositor under the forced-offscreen platform while
  normal platform sessions still use `TRenderer`.
- Deprecated-API evidence was refreshed on 2026-07-01: a clean
  `mise run build-qt6-deprecated-api` build compiled and linked `OpenToonz`
  with no Qt deprecation errors after the script level I/O and forced-offscreen
  Rasterizer/Renderer fallback slices.
- The build documentation now makes the lane split clearer: Qt 5.15.18 remains
  the default Apple Silicon package bridge, while Qt 6 validation stays in the
  dedicated Nix/mise lane until it is ready to replace the Qt 5 bridge.

## Current Functional Gaps

### Build, CI, And Packaging

- Fix the Linux AppImage compile failure in `iwa_bokehreffx.cpp` by including
  the header that defines `UCHAR_MAX`, then rerun the experimental workflow.
- Land the experimental workflow runner/module fixes on `master`, or otherwise
  ensure the scheduled default-branch workflow uses the same package setup as
  the current Qt 6 branch.
- Rerun `Qt 6 Experimental Binary Builds` and require Linux x86_64 AppImage,
  Windows x64 package, and macOS arm64 DMG artifacts before treating package
  validation as broadly unblocked.
- Keep regular Linux, Windows, and macOS Qt 6 branch CI green at the latest
  source-changing branch tip.
- Smoke-test the latest macOS arm64 experimental DMG for package mechanics,
  runtime data, Qt plugins, multimedia backends, and basic scene lifecycle.
  This is useful package evidence, but it does not close Windows or Linux
  package readiness.
- Enable `QT6_PROJECT_TOKEN` so the scheduled Project sync can update the
  GitHub Project from `.github/qt6-validation/manual-goals.json`.

### Scripting And Automation Evidence

- The script-smoke frontier is now broad enough to use as preflight evidence,
  but representative studio/user scripts still need to run against the Qt 6
  QJSEngine runtime and be compared with Qt 5 where practical.
- Manual Script Console coverage is still needed beyond the app-side smoke:
  broader interactive editing, `run()` error paths from the GUI console,
  `view(Image)`, `view(Level)`, FlipBook cleanup, repeated sessions, stale
  state cleanup, and real external scripts.
- Keep the Qt Script/QJSEngine split, script-smoke registry, GUI-smoke
  registry, and deprecated-API build lane green as new fixtures are added.
- Finish porting or explicitly retiring remaining object binding groups that
  are still partial or Qt 5 oriented.

### Viewer, Rendering, And Visual Parity

- Forced-offscreen Rasterizer and Renderer script smokes now have fallback
  coverage, but real platform sessions still need visual parity checks against
  Qt 5 for representative raster, vector, Toonz Raster, composite, and
  FX-heavy scenes.
- Preview and FX Preview UI workflows still need direct product validation:
  File > Preview, schematic FX Preview, live playback and timer behavior,
  sub-camera dragging, save UI, overwrite/warning/cancel flows, invalid output
  paths, and production FX-heavy scenes.
- Final render parity still needs representative production scenes across
  raster, vector, Toonz Raster, composite, frame-sequence, and FX-heavy cases.
- High-DPI behavior needs real UI sessions beyond the existing framebuffer
  probes, especially mixed-DPI or external-display behavior where available.
- OpenGL fallback messages and any visual differences from the new
  forced-offscreen fallbacks need triage if they appear in product workflows.

### Manual And Studio Validation

- Real operating-system mouse and cursor delivery still needs a frontmost user
  session. App-side and synthetic smokes do not prove the OS event path.
- Hardware tablet and stylus validation remains open for pressure, tilt, hover,
  eraser, side buttons, device switching, latency, and Windows pointer-input
  behavior.
- Product-level audio workflows remain manual: recording, save, insert,
  audible playback, permission prompts, cancellation, selected output route,
  and lip-sync timing. Microphone timeout skips are not recording parity.
- Camera and stop-motion workflows remain manual or studio checks: permission
  prompts, live preview, still capture, scene insertion, device changes, and
  hotplug behavior.
- Runtime translation packaging and language switching remain release checks.
- A release-candidate signoff matrix still needs supported platforms, Qt 5
  baseline, representative scenes, known limitations, blockers, docs, and
  troubleshooting criteria.

## Priority Notes

1. Experimental packaging remains the highest release blocker: Windows and
   Linux still fail before usable cross-platform artifacts exist.
2. The Linux `UCHAR_MAX` failure is the smallest direct source fix now visible.
3. The Windows runner/generator mismatch must reach the scheduled
   default-branch workflow before Windows package status is meaningful.
4. Script compatibility evidence is much stronger after the user-style fixture
   and forced-offscreen fallbacks; the next manual step is real user scripts
   and longer Script Console sessions.
5. GitHub Project sync plumbing is active, but the missing token still prevents
   live Project updates.
