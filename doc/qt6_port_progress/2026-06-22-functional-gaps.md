# Qt 6 Port Functional Gaps Report - 2026-06-22

## Snapshot

- Report date: 2026-06-22
- Baseline branch: `master` at `25677008e`
- Qt 6 branch reviewed: `codex/qt-6-port` at `dabf13796`
- Existing Qt 6 reference docs reviewed from `codex/qt-6-port`:
  - `doc/qt6_migration_study.md`
  - `doc/qt6_migration_goal_prompt.md`
  - `doc/qt6_remaining_work_and_manual_verification.md`

This report tracks the separate Qt 6 branch and the work still needed before
the port can be treated as release-ready. It does not mean `master` contains
the Qt 6 port implementation.

## Current CI And Workflow Evidence

- The latest regular `codex/qt-6-port` branch builds observed on 2026-06-22
  are green for Linux, macOS, and Windows at `dabf13796`.
- The latest observed `Qt 6 Experimental Binary Builds` workflow result is
  still the 2026-06-15 scheduled failure. The 2026-06-22 scheduled run had not
  started at the report query time.
- The experimental workflow has moved past the earlier Linux aqt module-list
  failure. The latest failure now has two package blockers:
  - Windows x64 failed during CMake configuration because the scheduled runner
    did not provide an instance matching the requested `Visual Studio 17 2022`
    generator.
  - Linux x86_64 AppImage failed while compiling `iwa_bokehreffx.cpp` because
    `UCHAR_MAX` is used without the required `<climits>` include.
- The same experimental run produced a successful macOS Apple Silicon arm64
  build, bundle verification, DMG creation, and artifact upload, but the
  rolling prerelease publish job was skipped because the workflow as a whole
  failed.
- The `Qt 6 Project Sync` workflow is still present and scheduled. The latest
  observed run completed successfully as a workflow, but skipped the actual
  Project update because `QT6_PROJECT_TOKEN` is not configured.
- Local build, package, GUI, and hardware validation were not rerun during this
  documentation pass.

## Progress Since The Previous Report

- The Qt 6 branch advanced from `b49ed0df5` to `dabf13796`, with green regular
  Linux, macOS, and Windows CI at the new tip.
- The old experimental aqt module-list blocker is no longer the current package
  failure; Qt 6 package installation reached Windows CMake configuration and
  Linux compilation in the latest observed run.
- The branch now includes a Qt 5 deprecated-API build guard task and a Qt 6
  deprecated-API build lane. The Qt 6 lane compiled and linked the
  `OpenToonz` executable with no Qt deprecation errors and no Qt-related
  deprecated-API warnings in the recorded branch documentation.
- Deprecated `QTime::start()` / `QTime::elapsed()` timing probes were replaced
  or guarded behind `QElapsedTimer` expectations.
- The branch added guardrail checks so Qt 6 preflight checks stay listed in
  the durable goal prompt and manual-verification command lists.
- The packaged GUI-smoke registry now checks aggregate task coverage, declared
  GUI smoke actions, and app-side smoke handlers.
- The preview high-DPI startup attribute path is guarded for Qt 6.
- The full app-side packaged Qt 6 GUI smoke aggregate has documented evidence
  from 2026-06-15, with microphone capture paths recorded as structured
  environment skips rather than parity evidence.

## Current Functional Gaps

### Build, CI, And Packaging

- Land the experimental workflow runner/module fixes on `master` or otherwise
  ensure the scheduled default-branch workflow uses the same package setup as
  the current Qt 6 branch.
- Fix the Linux AppImage compile failure in `iwa_bokehreffx.cpp` by including
  the header that defines `UCHAR_MAX`, then rerun the experimental workflow.
- Rerun `Qt 6 Experimental Binary Builds` and require Linux x86_64 AppImage,
  Windows x64 package, and macOS arm64 DMG artifacts before treating package
  validation as unblocked.
- Confirm whether the next Windows package run uses a runner with the expected
  Visual Studio generator or updates the generator to match the available
  runner image.
- Keep regular Linux, Windows, and macOS Qt 6 branch CI green at the latest
  branch tip.
- Enable `QT6_PROJECT_TOKEN` so the scheduled Project sync can update the
  GitHub Project from `.github/qt6-validation/manual-goals.json`.

### Build Guards And Deprecated API Evidence

- Keep the Qt 5 and Qt 6 deprecated-API build lanes reproducible and rerun
  them after shared source changes.
- Treat the new Qt 6 deprecated-API build result as strong compile evidence,
  but not as UI or production-scene parity.
- Continue using grep-based scope guards alongside compile-based deprecated API
  evidence so new Qt 5-only APIs do not re-enter shared Qt 6 code.

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
- QJSEngine script compatibility has stronger automated evidence than most UI
  areas, including bounded and natural-exit smokes plus script-smoke registry
  guarding, but external user scripts and broader interactive Script Console
  behavior still need product validation.

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

1. Regular branch CI is green, so the highest build-side blocker is now
   experimental packaging rather than ordinary compile/build health.
2. The Linux `UCHAR_MAX` compile failure is the clearest source fix to make
   before the next package rerun.
3. The Windows experimental workflow also needs the default-branch scheduled
   runner/generator mismatch resolved.
4. GitHub Project sync plumbing is active, but the missing token still prevents
   live Project updates.
5. Human and studio testing should remain focused on real input, hardware
   devices, Preview/FX Preview UI, production rendering, package runtime
   behavior, and cross-platform release polish.
