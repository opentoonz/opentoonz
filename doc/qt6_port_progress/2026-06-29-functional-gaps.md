# Qt 6 Port Functional Gaps Report - 2026-06-29

## Snapshot

- Report date: 2026-06-29
- Baseline branch: `master` at `0e173807c`
- Qt 6 branch reviewed: `codex/qt-6-port` at `c4d5daf48`
- Existing Qt 6 reference docs reviewed from `codex/qt-6-port`:
  - `doc/qt6_migration_study.md`
  - `doc/qt6_migration_goal_prompt.md`
  - `doc/qt6_remaining_work_and_manual_verification.md`

This report tracks the separate Qt 6 branch and the work still needed before
the port can be treated as release-ready. It does not mean `master` contains
the Qt 6 port implementation.

## Current CI And Workflow Evidence

- The latest regular `codex/qt-6-port` branch builds observed on 2026-06-27
  are green for Linux, macOS, and Windows at `c4d5daf48`.
- The latest observed `Qt 6 Experimental Binary Builds` workflow result is
  still the 2026-06-22 scheduled failure. The 2026-06-29 scheduled run had not
  started at the report query time.
- The experimental package blockers remain:
  - Windows x64 failed during CMake configuration because the scheduled runner
    did not provide an instance matching the requested `Visual Studio 17 2022`
    generator.
  - Linux x86_64 AppImage failed while compiling `iwa_bokehreffx.cpp` because
    `UCHAR_MAX` is used without the required `<climits>` include. The current
    Qt 6 branch still lacks that include.
- The same experimental run produced a successful macOS Apple Silicon arm64
  build, bundle verification, DMG creation, and artifact upload, but the
  rolling prerelease publish job was skipped because the workflow as a whole
  failed.
- The current Qt 6 branch workflow differs from `master`: it uses the Windows
  2022 runner and the expanded Qt module list, while the scheduled workflow
  still runs from the default branch. Those workflow fixes need to reach the
  scheduled workflow path before Windows package results can be trusted.
- The `Qt 6 Project Sync` workflow is still present and scheduled. The latest
  observed run completed successfully as a workflow, but skipped the actual
  Project update because `QT6_PROJECT_TOKEN` is not configured.
- Local build, package, GUI, and hardware validation were not rerun during this
  documentation pass.

## Progress Since The Previous Report

- The Qt 6 branch advanced from `dabf13796` to `c4d5daf48`, with green regular
  Linux, macOS, and Windows CI at the new tip.
- QJSEngine compatibility coverage expanded again:
  - global script output now covers `print()` / `warning()` formatting for
    strings, numbers, booleans, `undefined`, `null`, arrays, the legacy `void`
    return object, `dummy()`, and `ToonzVersion`.
  - `run(FilePath)` now covers child-script lookup through wrapped and copied
    `FilePath` values, returned arrays, child-script output, and global
    persistence across repeated child script execution.
  - FilePath coverage now includes mutable setters, parent conversion,
    metadata, copy construction after mutation, string coercion, concatenation,
    directory-listing wrappers, and stricter arity/path argument checks.
  - Scene coverage now includes column mutation edge cases and level-name
    coercion/lookup behavior.
  - Level coverage now includes name coercion, visibility through
    `Scene.getLevels()`, and current lookup behavior after wrapper-property
    renames.
  - Image coverage now includes load chaining and replacement with a second
    raster image of different dimensions.
  - Transform coverage now includes non-mutating chaining with fresh wrapper
    ids plus finite-number validation.
- Multimedia guard coverage tightened: Qt 5 audio-format APIs and legacy video
  surface APIs remain confined to audited Qt 5-only paths, while the active
  Qt 6 stop-motion/Pencil Test source path avoids those APIs.
- The branch still records strong deprecated-API build evidence from the June
  21-22 deprecated-API lanes, with no Qt deprecation errors in the Qt 6
  `OpenToonz` target.

## Current Functional Gaps

### Build, CI, And Packaging

- Land the experimental workflow runner/module fixes on `master` or otherwise
  ensure the scheduled default-branch workflow uses the same package setup as
  the current Qt 6 branch.
- Fix the Linux AppImage compile failure in `iwa_bokehreffx.cpp` by including
  the header that defines `UCHAR_MAX`, then rerun the experimental workflow.
- Rerun `Qt 6 Experimental Binary Builds` and require Linux x86_64 AppImage,
  Windows x64 package, and macOS arm64 DMG artifacts before treating package
  validation as broadly unblocked.
- The latest macOS arm64 DMG artifact gives a useful package-smoke target, but
  it does not close cross-platform package readiness while Windows and Linux
  packages fail.
- Keep regular Linux, Windows, and macOS Qt 6 branch CI green at the latest
  branch tip.
- Enable `QT6_PROJECT_TOKEN` so the scheduled Project sync can update the
  GitHub Project from `.github/qt6-validation/manual-goals.json`.

### Scripting And Automation Evidence

- The script-smoke frontier moved from basic bindings toward edge cases and
  user-style workflows. The remaining gap is less about missing synthetic
  coverage and more about running real user scripts and longer interactive
  Script Console sessions.
- Keep the forced-offscreen OpenGL gap explicit: a normal macOS
  LaunchServices-started QApplication script smoke validates Rasterizer and
  Renderer fixtures with a platform OpenGL context, while forced offscreen
  full-color Rasterizer/Renderer still reaches the documented unsupported GL
  path.
- Continue guarding the Qt Script/QJSEngine split, script-smoke registry, and
  GUI-smoke registry as new fixtures are added.

### Codex-Smoke Candidates

- Startup and workspace lifecycle remain good Codex GUI smoke candidates once
  a current package or local bundle is available: isolated profile launch,
  scene create/save/reopen, room switching, and basic panel redraw.
- Viewer and rendering parity still need current Qt 6 package evidence against
  Qt 5: blank/nonblank viewer state, stale framebuffer behavior, high-DPI
  alignment, raster/vector/Toonz Raster scenes, composite scenes, and FX-heavy
  scenes.
- Selection and Animate/Edit tool paths can receive another Codex GUI smoke
  pass before human signoff, but real OS-level cursor and drag behavior remain
  manual checks.
- Timeline/xsheet and palette/style workflows remain ready for Codex smoke
  preflight before human validation.

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
- Preview and FX Preview UI workflows still need direct product validation:
  File > Preview, schematic FX Preview, live playback and timer behavior,
  sub-camera dragging, save UI, overwrite and warning dialogs, cancellation,
  invalid output paths, and production FX-heavy preview scenes.
- Final render parity still needs representative production scenes across
  raster, vector, Toonz Raster, composite, frame-sequence, and FX-heavy cases.
- Runtime translation packaging and language switching remain release checks.

## Priority Notes

1. Regular branch CI remains green, so the highest build-side blocker is still
   experimental packaging rather than ordinary compile/build health.
2. The Linux `UCHAR_MAX` compile failure remains the clearest source fix to
   make before the next package rerun.
3. The Windows experimental workflow still needs the default-branch scheduled
   runner/generator mismatch resolved.
4. Script compatibility evidence is improving quickly; the next useful manual
   step is running real user scripts and longer Script Console workflows.
5. GitHub Project sync plumbing is active, but the missing token still prevents
   live Project updates.
