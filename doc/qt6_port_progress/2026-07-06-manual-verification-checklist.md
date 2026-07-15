# Qt 6 Manual Verification Checklist - 2026-07-06

This checklist tracks manual and field validation still needed for the
OpenToonz Qt 6 port. Update checklist items in this thread as verification
owners complete passes; the machine-readable companion is
`.github/qt6-validation/manual-goals.json`.

No new user-provided manual verification completions were found in this thread
since the 2026-06-29 report.

Field format per item:

- Status: one of `Blocked by build`, `Ready for Codex smoke`,
  `Codex evidence ready`, `Needs human pass`, `Failed`, `Accepted`, or
  `Studio field test`.
- Verifier: person, team, or `TBD`.
- Date: verification date or `TBD`.
- Notes: evidence, blocker, artifact, or follow-up.

## Build And Package Readiness

- [x] **Validate latest pushed Qt 6 branch commit**
  - Status: Accepted
  - Verifier: Codex automation
  - Date: 2026-07-06
  - Notes: `codex/qt-6-port` was reviewed at `419ef986f`. The latest visible
    regular Linux, macOS, and Windows branch builds succeeded for source
    commit `a115837f5` on 2026-07-01; later commits `3756c93d8` and
    `419ef986f` are documentation/build-evidence updates.

- [x] **Record Qt 5 and Qt 6 deprecated-API build evidence**
  - Status: Accepted
  - Verifier: Codex automation
  - Date: 2026-07-01
  - Notes: Branch documentation records a clean
    `mise run build-qt6-deprecated-api` build compiling and linking
    `OpenToonz` with no Qt deprecation errors after the script level I/O and
    forced-offscreen Rasterizer/Renderer fallback slices.

- [ ] **Rerun Qt 6 Experimental Binary Builds after current package fixes**
  - Status: Blocked by build
  - Verifier: TBD
  - Date: TBD
  - Notes: Latest observed run is the 2026-06-29 scheduled failure. Windows
    failed during CMake configuration, Linux failed during AppImage
    compilation, and macOS arm64 built, verified, created a DMG, and uploaded
    an artifact. The prerelease publish job was skipped because the workflow
    failed overall.

- [ ] **Fix Linux AppImage bokeh reference compile failure**
  - Status: Blocked by build
  - Verifier: TBD
  - Date: TBD
  - Notes: Linux x86_64 AppImage failed compiling `iwa_bokehreffx.cpp` because
    `UCHAR_MAX` is used without the header that defines it. The current Qt 6
    branch still lacks the `<climits>` include.

- [ ] **Land default-branch Qt 6 experimental workflow runner fix**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: The latest scheduled workflow used the default-branch workflow and
    failed Windows CMake configuration when `Visual Studio 17 2022` could not
    be found. The current Qt 6 branch workflow uses the Windows 2022 runner
    and expanded Qt module list; the scheduled workflow must pick up the
    correct runner or generator before Windows packages can be trusted.

- [ ] **Smoke-test latest macOS arm64 experimental DMG**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: The 2026-06-29 experimental workflow produced a macOS arm64 DMG
    artifact even though the workflow failed overall. Use it for package
    mechanics and runtime-data smoke, but do not treat it as cross-platform
    release readiness.

- [ ] **Verify current Qt 6 packaged artifacts on every supported platform**
  - Status: Blocked by build
  - Verifier: TBD
  - Date: TBD
  - Notes: Package validation depends on fresh Linux x64, Windows x64,
    macOS arm64, and macOS x64 artifacts from the experimental binary workflow.
    Include runtime data, plugins, helper executables, multimedia backends, and
    OpenGL behavior.

- [ ] **Enable GitHub Project sync token**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: The `Qt 6 Project Sync` workflow is present and scheduled, and the
    JSON source exists. The latest observed 2026-07-06 run skipped Project
    updates because `QT6_PROJECT_TOKEN` is not configured.

## Startup And Workspace

- [ ] **Launch packaged Qt 6 app and verify scene lifecycle**
  - Status: Ready for Codex smoke
  - Verifier: TBD
  - Date: TBD
  - Notes: Codex can preflight startup, create scene, save, close, relaunch,
    reopen, and basic room/panel redraw once a current package or local bundle
    is available.

- [ ] **Verify rooms, docking, runtime data, icons, and stylesheets**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Check room switching, dock/undock, default projects, QSS, shaders,
    brushes, presets, icons, and plugin/resource loading from the package.

## Viewer And Rendering

- [ ] **Compare Qt 5 and Qt 6 viewer output on representative scenes**
  - Status: Ready for Codex smoke
  - Verifier: TBD
  - Date: TBD
  - Notes: Start with Codex screenshot/pixel comparisons for raster, vector,
    Toonz Raster, composite, and FX-heavy scenes, then escalate mismatches to
    human review. Forced-offscreen script fallbacks are improving, but they do
    not replace real viewer parity checks.

- [ ] **Record forced-offscreen Rasterizer and Renderer fallback evidence**
  - Status: Codex evidence ready
  - Verifier: Codex automation
  - Date: 2026-07-06
  - Notes: Branch documentation records passing focused forced-offscreen
    Rasterizer and Renderer script smokes. Rasterizer falls back to
    `TRasterImageUtils::vectorToFullColorImage()` under
    `QT_QPA_PLATFORM=offscreen`; Renderer uses a camera-sized raster compositor
    for simple script-created raster/vector cells under the forced-offscreen
    platform.

- [ ] **Verify high-DPI visual behavior in real UI sessions**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Existing framebuffer probes are not enough. Check viewer alignment,
    input positions, overlays, zoom/pan, and mixed-DPI or external-display
    behavior where available.

- [ ] **Run manual Preview and FX Preview UI workflows**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Cover File > Preview, schematic FX Preview command, playback/timer
    behavior, sub-camera dragging, saved-frame UI, overwrite/warning/cancel
    flows, invalid output paths, and production FX-heavy scenes.

- [ ] **Verify final render parity on production scenes**
  - Status: Studio field test
  - Verifier: TBD
  - Date: TBD
  - Notes: Render single frame, frame sequence, composite columns, vector
    levels, Toonz Raster levels, and FX-heavy scenes; compare output against
    Qt 5 and external viewers.

## Drawing, Input, And Tools

- [ ] **Verify real OS-level mouse and cursor delivery**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Prior app-side and synthetic smokes do not prove real frontmost
    desktop event delivery. Exercise drawing, transform drags, cursor feedback,
    context menus, and repeated input.

- [ ] **Exercise raster and vector drawing workflows**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Draw repeated strokes, undo/redo, save/reopen, redraw, compare with
    Qt 5, and include both mouse and tablet where possible.

- [ ] **Verify Selection and Animate/Edit tool parity**
  - Status: Ready for Codex smoke
  - Verifier: TBD
  - Date: TBD
  - Notes: Codex can preflight several app-side selection and transform paths,
    but human confirmation is still needed for real input and cursor behavior.

## Timeline, Xsheet, Guides, And Overlays

- [ ] **Verify broader timeline and xsheet interactions**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Scrub, select cells, move columns, edit notes, use context menus,
    pan/auto-pan, and compare with Qt 5.

- [ ] **Verify onion skin, Shift and Trace, guides, rulers, and overlays**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Cover manual marker workflows, fixed and relative markers, colors,
    orientations, camera box, safe area, field guides, ruler/guide variants,
    and overlay coexistence with drawing and room changes.

## Scripting

- [ ] **Keep QJSEngine script fixtures green and expand advanced API coverage**
  - Status: Codex evidence ready
  - Verifier: TBD
  - Date: TBD
  - Notes: Coverage now includes the user-style workflow fixture, Scene I/O
    edges, level path loading, level transformer wrapper lifetime, ImageBuilder
    edge behavior, ToonzRasterConverter lifecycle behavior, vectorizer
    properties, Rasterizer edges, Renderer edges/lifecycle behavior, and
    forced-offscreen Rasterizer/Renderer fallbacks. Next coverage should keep
    targeting remaining partial object bindings and real external scripts.

- [ ] **Verify real user scripts against Qt 6 QJSEngine**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: The branch now has a synthetic user-style fixture, but this does
    not replace representative studio/user scripts. Include path-heavy,
    scene-mutation, renderer, wrapper-lifetime, and error-path scripts where
    possible.

- [ ] **Verify Script Console GUI behavior manually**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: App-side smoke now covers `print`, `warning`, a `run()` happy path
    with child output/return/global persistence, expected `view()` error
    display, and one Up-arrow history recall path. Manual coverage should still
    include broader command editing, `run()` error paths, `view(Image)`,
    `view(Level)`, FlipBook cleanup, repeated sessions, stale-state cleanup,
    and real user scripts.

## Audio, Camera, Stop-Motion, And Tablet

- [ ] **Verify audio recording, playback, and lip-sync through product UI**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Backend and WAV-writer smokes are preflight only. Structured
    microphone timeout skips are not recording parity. Confirm recording, save,
    insert, audible playback, permission prompts, cancellation, selected output
    route, and lip-sync timing.

- [ ] **Verify camera preview and stop-motion capture on real hardware**
  - Status: Studio field test
  - Verifier: TBD
  - Date: TBD
  - Notes: Cover device enumeration, live preview, still capture, scene
    insertion, device change, and hotplug behavior.

- [ ] **Verify tablet and stylus hardware behavior**
  - Status: Studio field test
  - Verifier: TBD
  - Date: TBD
  - Notes: Check pressure, tilt, hover, eraser, side buttons, device switching,
    latency, and Windows pointer-input bridge behavior. Synthetic tablet
    events are not enough.

## Release And Documentation

- [ ] **Verify runtime translation and language switching**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Build translations, package them, switch UI language, restart, and
    spot-check localized UI. The Qt 6 translation build lane has prior
    evidence, but packaged runtime language switching remains a manual release
    check.

- [ ] **Define release-candidate validation matrix and signoff criteria**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Include supported platforms, Qt 5 baseline version, representative
    production scenes, known limitations, release blockers, docs, and
    troubleshooting updates.
