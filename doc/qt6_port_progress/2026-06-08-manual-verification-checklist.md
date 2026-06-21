# Qt 6 Manual Verification Checklist - 2026-06-08

This checklist tracks manual and field validation still needed for the
OpenToonz Qt 6 port. Update checklist items in this thread as verification
owners complete passes; the machine-readable companion is
`.github/qt6-validation/manual-goals.json`.

Field format per item:

- Status: one of `Blocked by build`, `Ready for Codex smoke`,
  `Codex evidence ready`, `Needs human pass`, `Failed`, `Accepted`, or
  `Studio field test`.
- Verifier: person, team, or `TBD`.
- Date: verification date or `TBD`.
- Notes: evidence, blocker, artifact, or follow-up.

## Build And Package Readiness

- [ ] **Restore macOS CI guard preflight**
  - Status: Blocked by build
  - Verifier: TBD
  - Date: TBD
  - Notes: Latest pushed `codex/qt-6-port` macOS CI failed before compile when
    the macOS runner could not find `rg` during a Qt 6 guard script. Linux and
    Windows CI passed for the same pushed commit.

- [ ] **Validate latest local Qt 6 branch commit**
  - Status: Blocked by build
  - Verifier: TBD
  - Date: TBD
  - Notes: Local `codex/qt-6-port` commit `ca86662aa` is newer than the latest
    pushed commit with CI results and needs CI or equivalent local build
    evidence.

- [ ] **Verify current Qt 6 packaged artifacts on every supported platform**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Validate Linux x64, Windows x64, macOS arm64, and macOS x64 package
    behavior from current Qt 6 artifacts. Include runtime data, plugins,
    helper executables, multimedia backends, and OpenGL behavior.

## Startup And Workspace

- [ ] **Launch packaged Qt 6 app and verify scene lifecycle**
  - Status: Ready for Codex smoke
  - Verifier: TBD
  - Date: TBD
  - Notes: Codex can preflight startup, create scene, save, close, relaunch,
    reopen, and basic room/panel redraw before a human pass.

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
    human review.

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
  - Notes: The branch has substantial script-smoke coverage. Next coverage
    should focus on remaining advanced scene APIs, more object bindings, and
    external scripts users depend on.

- [ ] **Verify Script Console GUI behavior manually**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Run basic commands, `view(Image)`, `view(Level)`, invalid argument
    cases, visible error reporting, and stale-state cleanup.

## Audio, Camera, Stop-Motion, And Tablet

- [ ] **Verify audio recording, playback, and lip-sync through product UI**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Backend and WAV-writer smokes are preflight only. Confirm recording,
    save, insert, audible playback, permission prompts, cancellation, selected
    output route, and lip-sync timing.

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

- [ ] **Define release-candidate validation matrix and signoff criteria**
  - Status: Needs human pass
  - Verifier: TBD
  - Date: TBD
  - Notes: Include supported platforms, Qt 5 baseline version, representative
    production scenes, known limitations, release blockers, docs, and
    troubleshooting updates.

- [ ] **Confirm GitHub Project sync consumes manual goals JSON**
  - Status: Ready for Codex smoke
  - Verifier: TBD
  - Date: TBD
  - Notes: The JSON source has been created for Project sync. Confirm the
    workflow that reads it is present and active before treating Project status
    as automated.
