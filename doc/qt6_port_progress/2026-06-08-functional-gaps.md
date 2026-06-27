# Qt 6 Port Functional Gaps Report - 2026-06-08

## Snapshot

- Report date: 2026-06-08
- Baseline branch: `master` at `069d3ba8`
- Qt 6 branch reviewed: `codex/qt-6-port` at local commit `ca86662aa`
- Latest pushed Qt 6 branch commit with CI results: `590702546`
- Existing Qt 6 reference docs reviewed from `codex/qt-6-port`:
  - `doc/qt6_migration_study.md`
  - `doc/qt6_migration_goal_prompt.md`
  - `doc/qt6_remaining_work_and_manual_verification.md`

This report is a progress artifact for the Qt 5 to Qt 6 transition. It does
not mean the `master` branch has the Qt 6 port code; it records the current
state of the separate `codex/qt-6-port` branch and the validation gaps that
still need to close before the port can be treated as release-ready.

## Current CI And Build Evidence

- Latest pushed `codex/qt-6-port` CI, run on 2026-06-05:
  - Linux Build: success.
  - Windows Build: success.
  - MacOS Build: failure.
- The latest macOS failure occurs during the preflight guard chain before a
  meaningful compile failure. The guard script reaches
  `scripts/qt6/check-checkbox-state-scope.sh`, cannot find `rg` in the macOS
  job environment, and then reports the checkbox helper missing.
- A prior pushed commit on 2026-06-05 had all three platform builds green, so
  the current macOS signal is best treated as a CI/tooling blocker until the
  macOS job installs or otherwise provides the guard dependencies and reruns.
- The latest local-only `codex/qt-6-port` commit adds broader Qt 6 mouse-event
  scope checks and documentation. It is newer than the latest pushed commit
  with CI results, so it still needs CI evidence.
- Local build, package, GUI, and hardware validation were not rerun during this
  documentation pass.

## Progress Already Evidenced On The Qt 6 Branch

- A separate Qt 6 lane exists through CMake, Nix, CMake presets, and mise while
  preserving the Qt 5 lane.
- The Qt 6 lane has targeted compatibility helpers and guard scripts for many
  removed or changed Qt APIs, including Core5Compat/text-codec scope,
  QRegExp, desktop widget APIs, font metrics, font database APIs, high-DPI
  startup attributes, multimedia, media player, touch/tablet events, QVariant,
  QKeySequence, combo box activation, checkbox state changes, QButtonGroup
  integer signals, wheel events, graphics-scene event positions, QAction
  parent-widget usage, QColor parsing, QImage mirroring, QGLFormat, QGL legacy
  classes, and mouse-event coordinates.
- The macOS packaged Qt 6 app has prior local evidence for startup, scene
  create/open, high-DPI probes, xsheet scrub, raster/vector viewer framebuffer
  output, preview render output, FX Preview cache output, final-render PNG
  output, overlay smokes, Animate/Edit and Selection tool app-side paths,
  vector/raster brush app-side paths, synthetic tablet events, multimedia
  backend probes, and a broad QJSEngine script fixture set.
- That automated smoke evidence is useful preflight, not product parity. The
  remaining risks are mostly in real UI workflows, operating-system input
  delivery, real devices, cross-platform packaging, and production scenes.

## Current Functional Gaps

### Build And CI

- Restore macOS CI preflight so guard scripts run consistently on the GitHub
  macOS runner.
- Push or otherwise validate the local-only `ca86662aa` Qt 6 branch commit.
- Keep Linux, Windows, and macOS Qt 5 and Qt 6 lanes green after every shared
  source change.
- Confirm the scheduled Qt 6 experimental binary workflow is active from the
  repository default branch before relying on weekly artifacts.
- Keep CI/build success separate from GUI parity. A green compile does not
  close product verification goals.

### Startup, Workspace, And Runtime Data

- Reverify startup and scene lifecycle from current packaged Qt 6 artifacts
  after the next green build.
- Check room switching, docking, undocking, panel redraw, startup popup,
  default projects, QSS, shaders, brushes, presets, icons, and plugin loading.
- Confirm packaged apps load Qt frameworks/plugins and runtime data from the
  package rather than from a developer environment.

### Viewer, Rendering, Preview, And FX Preview

- Compare Qt 5 and Qt 6 viewer output with representative raster, vector,
  Toonz Raster, composite, and FX-heavy scenes.
- Verify real high-DPI visual behavior beyond the existing framebuffer probes.
- Run the manual File > Preview path, schematic FX Preview command path, live
  FX Preview playback/timer behavior, sub-camera dragging, saved-frame UI,
  overwrite/warning/cancel flows, invalid output paths, and production
  FX-heavy preview scenes.
- Continue blank-viewer, stale-frame, OpenGL fallback, cache output, and visual
  parity regression checks with realistic scenes.

### Drawing, Input, Selection, And Tooling

- Verify real operating-system mouse and cursor delivery. Prior app-side and
  synthetic smokes do not prove frontmost desktop event delivery.
- Exercise repeated raster and vector drawing with undo/redo, save/reopen, and
  redraw checks.
- Verify multi-object and multi-frame selection workflows, transform workflows,
  cursor artwork outside the checked Animate/Edit cursor set, and real
  OS-level transform dragging.
- Compare drawing and selection behavior against a Qt 5 baseline.

### Timeline, Xsheet, Onion Skin, Guides, And Rulers

- Run broader manual xsheet and timeline interactions through real mouse input.
- Verify onion-skin workflows beyond the app-side row-area, fixed-marker,
  context, color, orientation, and Shift and Trace coverage.
- Verify ruler/guide variants beyond app-side create, move, delete, and
  drag-hide smokes.
- Check overlay coexistence across rooms, panels, DPI settings, stylesheets,
  zoom/pan, drawing, and selection.

### Scripting

- Keep the current QJSEngine fixture suite green in bounded and natural-exit
  modes.
- Extend coverage into remaining advanced scene APIs, additional object
  bindings, and external user scripts.
- Verify Script Console GUI behavior, `view(Image)`, `view(Level)`, visible
  error reporting, and stale-state cleanup manually.

### Audio, Camera, And Stop-Motion

- Run product-level audio recording through the UI, including save, insert,
  playback, permission prompts, cancellation, selected output route, and
  lip-sync timing.
- Run camera preview and still capture on real hardware, including device
  change and hotplug behavior.
- Treat backend enumeration and WAV-writer smokes as preflight only.

### Tablet And Stylus

- Validate pressure, tilt, hover, eraser, side buttons, device switching, and
  latency on real tablet/stylus hardware.
- Confirm Windows pointer-input bridge behavior on Windows hardware.
- Keep synthetic tablet-event smoke results separate from hardware parity.

### Cross-Platform Packaging And Release Polish

- Validate Linux AppImage, Windows package, and macOS DMG artifacts from the
  Qt 6 branch.
- Re-audit Qt 6 module lists, deploy-tool arguments, plugins, multimedia
  backends, runtime library paths, helper executables, file associations, and
  OpenGL behavior per platform.
- Recheck macOS signing, notarization, helper executables, and copied runtime
  data for release candidates.
- Define the release-candidate build matrix, Qt 5 baseline, representative
  production scenes, known limitations, and release signoff criteria.

## Priority Notes

1. The first actionable blocker is the macOS CI preflight environment. Without
   that, the current branch has only Linux and Windows CI evidence for the
   latest pushed Qt 6 state.
2. The next Codex-friendly work should smoke-test current packaged artifacts
   where automation can provide evidence before handing workflows to human or
   studio validation.
3. Human/studio verification should focus first on OS-level input, tablet,
   product audio/camera, Preview/FX Preview UI, and production-scene visual
   parity.
