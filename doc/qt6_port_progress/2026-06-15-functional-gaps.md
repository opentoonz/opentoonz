# Qt 6 Port Functional Gaps Report - 2026-06-15

## Snapshot

- Report date: 2026-06-15
- Baseline branch: `master` at `765f99525`
- Qt 6 branch reviewed: `codex/qt-6-port` at `b49ed0df5`
- Existing Qt 6 reference docs reviewed from `codex/qt-6-port`:
  - `doc/qt6_migration_study.md`
  - `doc/qt6_migration_goal_prompt.md`
  - `doc/qt6_remaining_work_and_manual_verification.md`

This report tracks the separate Qt 6 branch and the work still needed before
the port can be treated as release-ready. It does not mean `master` contains
the Qt 6 port implementation.

## Current CI And Workflow Evidence

- The latest regular `codex/qt-6-port` branch builds observed on 2026-06-12
  are green for Linux, macOS, and Windows at `b49ed0df5`.
- The June 8 macOS preflight blocker from the previous report is superseded by
  later green macOS branch CI.
- The latest observed `Qt 6 Experimental Binary Builds` workflow result is
  still the 2026-06-08 failure. It failed before producing packages because
  the Linux aqt install step could not resolve `qtdeclarative`, `qtsvg`, and
  `qttools` for the configured Qt version. Later branch commits restore module
  handling and centralize preflight checks, but a fresh experimental run is
  still needed before package validation can proceed.
- The `Qt 6 Project Sync` workflow is present and scheduled. The latest
  observed run completed successfully as a workflow, but skipped the actual
  Project update because `QT6_PROJECT_TOKEN` is not configured.
- Local build, package, GUI, and hardware validation were not rerun during this
  documentation pass.

## Progress Since The Previous Report

- macOS Qt 6 guard preflight evidence improved from blocked to green in the
  regular branch CI lanes.
- The Qt 6 branch now includes fixes around macOS SDK detection for macOS 26
  toolchains and the weekly Qt 6 build preflight.
- Qt 6 workflow modules and shared preflight checks were restored and
  centralized after the experimental binary workflow module-list failure.
- GUI smoke regressions after strict binding validation were fixed.
- The script-smoke registry guard now verifies that aggregate script-smoke
  tasks and fixture files stay registered.
- Strict Qt 6 script smoke guards were tightened.
- The Qt runtime smoke lock moved to the build directory so smoke runs do not
  depend on a shared source-tree lock.

## Current Functional Gaps

### Build, CI, And Packaging

- Rerun `Qt 6 Experimental Binary Builds` after the aqt module handling fixes
  and confirm Linux, Windows, and macOS package artifacts are produced.
- Keep regular Linux, Windows, and macOS Qt 6 branch CI green at the latest
  branch tip.
- Enable `QT6_PROJECT_TOKEN` so the scheduled Project sync can update the
  GitHub Project from `.github/qt6-validation/manual-goals.json`.
- Validate package runtime data and plugin loading from produced artifacts:
  Qt frameworks, Qt plugins, multimedia backends, `stuff`, rooms, QSS, shaders,
  brushes, presets, helper executables, file associations, and OpenGL behavior.

### Codex-Smoke Candidates

- Startup and workspace lifecycle can be preflighted again now that regular
  branch CI is green: isolated profile launch, scene create/save/reopen, room
  switching, and basic panel redraw.
- Viewer and rendering parity still need current Qt 6 package evidence against
  Qt 5: blank/nonblank viewer state, stale framebuffer behavior, high-DPI
  alignment, raster/vector/Toonz Raster scenes, composite scenes, and FX-heavy
  scenes.
- Selection and Animate/Edit tool paths can receive another Codex GUI smoke
  pass before human signoff, but real OS-level cursor and drag behavior remain
  manual checks.
- Timeline/xsheet and palette/style workflows are ready for Codex smoke
  preflight before human validation.
- QJSEngine script compatibility has strong automated evidence, including
  bounded and natural-exit script smokes plus registry guarding, but Script
  Console GUI behavior and external user scripts still need product validation.

### Manual And Studio Validation

- Real operating-system mouse and cursor delivery still needs a frontmost user
  session. App-side and synthetic smokes do not prove the OS event path.
- Hardware tablet and stylus validation remains open for pressure, tilt, hover,
  eraser, side buttons, device switching, latency, and Windows pointer-input
  behavior.
- Product-level audio workflows remain manual: recording, save, insert,
  audible playback, permission prompts, cancellation, selected output route,
  and lip-sync timing.
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

1. The highest build-side blocker is now the experimental binary workflow, not
   regular branch CI.
2. The GitHub Project sync plumbing is active, but the missing token means the
   Project is not yet a live reflection of the JSON goals.
3. The best next automation work is to rerun Codex GUI smokes from current
   packages once experimental artifacts exist.
4. Human and studio testing should focus on real input, hardware devices,
   Preview/FX Preview UI, production rendering, package runtime behavior, and
   cross-platform release polish.
