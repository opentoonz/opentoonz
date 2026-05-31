# Codex Goal Prompt: OpenToonz Qt 6 Migration

Use this prompt to start a long-running Codex implementation session in this
checkout.

## Goal Prompt

You are working in the OpenToonz repository root.

Your objective is to turn the Qt 6 migration study into an incremental,
verified migration path. Preserve the current Qt 5 build while adding a Qt 6
lane. Do not do a big-bang port. The Metal migration is no longer a sequencing
blocker for this branch; finish the Qt 6 port first, including viewer,
rendering, drawing, and visual-parity work, then revisit Metal after Qt 6 is
complete.

Read these first:

1. `AGENTS.md`
2. `doc/qt6_migration_study.md`
3. `doc/how_to_build_nix_mise.md`
4. `toonz/sources/CMakeLists.txt`
5. `toonz/sources/CMakePresets.json`
6. `nix/opentoonz-env.nix`
7. `mise.toml`
8. `scripts/macos/package-nix-app.sh`
9. `.github/workflows/workflow_macos.yml`
10. `.github/workflows/workflow_linux.yml`
11. `.github/workflows/workflow_windows.yml`

## Immediate Sequencing Rule

Treat Qt 6 as the active priority and park Metal-specific follow-up until the
Qt 6 port is complete. Viewer, renderer, offscreen GL, shader, and tool overlay
rendering files are now in scope for Qt 6 work when they move the port toward
visual and workflow parity. Keep edits incremental and preserve the Qt 5 lane,
but do not defer Qt 6 rendering work solely because a Metal draft exists.

## Non-Negotiable Constraints

- Keep `mise run configure` and the default Qt 5 lane working.
- Keep Nix as the dependency owner and mise as the task runner.
- Do not replace Qt 5 globally in the first slice.
- Do not remove Qt Script behavior without a compatibility plan and fixtures.
- Do not paper over missing Qt 6 APIs by disabling user-visible features unless
  the limitation is explicitly documented as temporary.
- Do not edit vendored third-party code or binary assets for this migration
  unless a specific Qt 6 blocker requires it.
- Keep changes small enough that another agent can continue from the result.
- In final handoffs, list changed files and validation actually run.

## Current Branch Status

As of May 30, 2026, the initial Qt 6 runway is already implemented in this
branch.

- `OPENTOONZ_QT_MAJOR` exists and defaults to the Qt 5 lane.
- Qt target, MOC, resource, and translation command selection is centralized.
- Nix, CMake presets, and mise tasks include a Qt 6 lane next to the Qt 5 lane.
- The Qt 5 app target links with
  `cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz --parallel`.
- The Qt 6 app target links with
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel`.
- Compile fixes already cover the first build frontier across CMake, screen
  helpers, Qt Script shell bring-up, audio, stop-motion camera enumeration,
  layout margin APIs, MOC-visible Qt-major guards, and stale MOC/header entries.
- Direct `QGLWidget::convertToGLFormat` usage has been removed from the tool
  code. `tnztools/toolutils.cpp` and `tnztools/skeletontool.cpp` now use the
  shared `QtCompat::convertToGLFormat` helper, keeping the image-to-OpenGL
  conversion behavior in one Qt-version-aware place.
- Direct `QTextCodec` usage is now isolated behind `ttextcodec.h`. The current
  adapter preserves the legacy Shift-JIS, GBK, UTF-8, and PSD layer-name paths
  while keeping the remaining Core5Compat dependency auditable.
- The text-codec bridge now has explicit Qt 5 and Qt 6 regression checks:
  `mise run check-textcodec` and `mise run check-textcodec-qt6`. They compile a
  small adapter user and verify Shift-JIS, GBK, and unknown-codec UTF-8 fallback
  behavior before any future Core5Compat reduction.
- Core5Compat is no longer part of the global `${QT_CORE_LINK_TARGETS}` set.
  It is now exposed as `${QT_CORE5COMPAT_LINK_TARGETS}` and linked only by
  `image`, `toonzlib`, and `OpenToonz`, the current targets that include the
  legacy text-codec adapter. Drop those targeted links once the adapter no
  longer needs Core5Compat.
- After that link-scope reduction, both app targets still build:
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel` and `cmake --build toonz/build/nix-relwithdebinfo --target
  OpenToonz --parallel`.
- The SXF direct-text popup now uses `QCheckBox::checkStateChanged` on Qt 6.7+
  and the existing `stateChanged` signal on Qt 5, removing a Qt 6 deprecation
  warning without changing the Qt 5 lane.
- The Qt 6 startup path no longer sets the removed/deprecated high-DPI
  application attributes; Qt 6 uses its always-on high-DPI behavior, while the
  Qt 5 lane keeps the existing attributes. The translator startup path also
  uses `QLibraryInfo::path()` on Qt 6 and explicitly discards optional
  `QTranslator::load()` results. After this cleanup, the current app-target
  warning frontier in `main.cpp` is limited to OpenGL/GLUT deprecation output.
- A bounded Qt 6 Cocoa startup smoke has been attempted with an isolated runtime
  `stuff` copy. It stayed alive until stopped by the smoke harness, but exposed
  plugin discovery gaps.
- Runtime plugin wiring is now in place for the current macOS Qt 6 lane: the
  Nix Qt plugin path includes QtSvg, macOS packaging recognizes Qt 6
  `multimedia` plugins plus SVG `iconengines`, and packaged plugins are
  rewritten to use bundled Qt frameworks instead of Nix-store Qt frameworks.
- `mise` includes Qt 6 macOS packaging and arm64 bundle-check tasks:
  `package-macos-qt6` and `check-macos-arm64-qt6`.
- `mise run package-macos-qt6` and `mise run check-macos-arm64-qt6` pass for
  the current macOS arm64 Qt 6 bundle.
- On 2026-05-25, post-relink validation reran the macOS Qt 6 package,
  arm64 bundle check, bounded startup smoke, non-rendering high-DPI diagnostic
  smoke, scene create/open smoke, app-side xsheet scrub smoke, media-device
  enumeration smoke, camera-format smoke, audio-input backend smoke,
  audio-output backend smoke, audio-recording WAV smoke, and audio-playback WAV
  smoke
  successfully against the current packaged app. The bundle check reports the
  main executable as arm64 and checks 291 packaged Mach-O files for arm64; the
  high-DPI smoke recorded window and screen DPR as 2.00. The audio-input smoke
  used the MacBook Pro Microphone at 48 kHz and captured 96,256 bytes with
  `audioError=NoError`. The audio-recording WAV smoke recorded 65,536 bytes at
  44.1 kHz, produced a 65,580-byte WAV, reloaded it through OpenToonz sound I/O,
  and measured 32,768 samples over 743 ms. The audio-playback WAV smoke
  generated a 3,000 ms, 132,300-sample mono WAV at 44.1 kHz, wrote 264,600 bytes
  through `AudioWriterWAV`, reloaded it through OpenToonz sound I/O, and started
  then stopped playback through `TSoundOutputDevice` on the default output.
- A bounded startup smoke of the packaged Qt 6 app now gets past the prior
  duplicate-Qt/platform-plugin abort and loads the Qt Multimedia FFmpeg backend.
  The repeatable task is `mise run gui-smoke-qt6`. After the latest relink and
  package pass, the packaged app still passes this bounded startup smoke.
- Bounded GUI and script smokes now use SIGKILL for harness cleanup so
  OpenToonz's SIGTERM crash handler does not generate misleading `atos` output.
  OpenGL fallback messages remain to be triaged during interactive GUI smoke.
- An earlier packaged Qt 6 interactive GUI smoke created a new scene in the
  isolated `gui-smoke-qt6` sandbox and then reopened that generated scene file
  through the command-line file argument. This caught and verified a fix for a
  Qt 6-exposed iterator invalidation crash in the shared `TThread` task
  scheduler during `IconGenerator` invalidation after scene creation.
- Scene create/open GUI automation is green through a narrow app-side smoke
  status hook. `mise run gui-smoke-scene-create-qt6` creates and saves a new
  `.tnz` scene in the isolated `gui-smoke-qt6` sandbox, and
  `OPENTOONZ_GUI_SMOKE_FILE=<path-to-scene.tnz> bash scripts/qt6/run-gui-smoke.sh`
  reopens that generated scene through the packaged Qt 6 app. The harness keeps
  its System Events fallback, but current Qt 6 validation should rely on the
  app-side status file because macOS System Events can report zero Qt 6
  OpenToonz windows even when Qt QPA logging confirms Cocoa created the main
  window and Startup popup. In sandboxed agent runs, AppleScript error `-10827`
  during scene-create smoke should be treated as a macOS automation/session
  access issue and rerun with GUI automation permission before calling it a Qt
  regression.
- A non-rendering packaged GUI xsheet smoke is now green through
  `mise run gui-smoke-xsheet-scrub-qt6`. It uses the app-side smoke hook to
  create a sandbox scene, create raster and vector levels, populate xsheet
  cells, switch frame and column state, save the scene, and verify the resulting
  frame/column/level counts. It deliberately does not validate drawing, viewer
  redraw, OpenGL fallback behavior, preview render, or final render.
- Packaged Qt 6 raster and vector viewer framebuffer smokes are now green
  through `mise run gui-smoke-viewer-render-qt6` and
  `mise run gui-smoke-viewer-vector-render-qt6`. They use the app-side smoke
  hook to create a sandbox scene, insert either a red raster frame or a red
  vector stroke into the xsheet, capture the `SceneViewer` framebuffer before
  and after the update, require changed and red-dominant pixels, and save the
  before/after captures beside the GUI smoke status file. These are narrow
  rendering guards; they do not yet validate brush input, interactive vector
  drawing, onion skin, tool overlays, FX preview, final render output, or Qt
  5-vs-Qt 6 visual parity.
- On 2026-05-30, the viewer framebuffer capture path was tightened to treat
  high-DPI sizing as part of the pass condition. The app-side status now records
  logical viewer size, device-pixel viewer size, `SceneViewer::getDevPixRatio()`,
  Qt widget DPR, screen DPR, a logical-to-device probe, and a
  framebuffer-to-viewer-size probe. `viewerRenderProbe=ok` now requires those
  probes in addition to changed and red-dominant pixels. After rebuild and
  packaging, the packaged Qt 6 viewer-render smoke passed with logical viewer
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, `771605` changed
  pixels, and `358756` red pixels. The packaged Qt 5 app passed the same smoke
  with the same measurements, giving this check a current dual-lane baseline.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-zoom-pan-qt6`. This app-side smoke inserts a red
  raster frame, captures the `SceneViewer` framebuffer, applies a `1.35` view
  transform plus a `48,-32` logical-pixel pan scaled through
  `SceneViewer::getDevPixRatio()`, and requires both `viewerRenderProbe=ok` and
  `viewerTransformProbe=ok`. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, scale
  `2.3297 -> 3.1451`, `1303367` changed pixels, and `648404` red pixels. The
  packaged Qt 5 run matched those measurements. This is a direct view-matrix
  zoom/pan and high-DPI framebuffer guard; it does not validate OS-level wheel,
  trackpad, or drag event delivery.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-onion-skin-qt6`. This app-side smoke creates two
  transparent raster levels in one xsheet column, captures the current row with
  onion skin disabled, enables whole-scene mobile onion skin at back offset
  `-1`, and requires `onionSkinProbe=ok`, `viewerRenderProbe=ok`, the high-DPI
  framebuffer probes, visible red current-frame pixels, one back onion stage
  player, and an onion-pixel increase. The packaged Qt 6 run reported logical
  viewer size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  `45651` red pixels, and onion pixels `0 -> 42012`. The packaged Qt 5 run
  matched those measurements. This is an app-side onion-skin framebuffer guard;
  it does not validate timeline onion marker UI, interactive onion toggles,
  custom onion colors, overlay placement, or full visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-camera-overlay-qt6`. This app-side smoke creates a
  blank sandbox scene, disables the camera box overlay through `MI_ViewCamera` /
  `viewCameraToggle`, captures the `SceneViewer`, enables the same overlay, and
  requires `cameraOverlayProbe=ok`, `viewerRenderProbe=ok`, the high-DPI
  framebuffer probes, changed pixels, red overlay pixels, and before/after
  captures. The packaged Qt 6 run reported logical viewer size `994x819`, DPR
  `2` / `2.00`, framebuffer `1988x1638`, `3638` changed pixels, and `3638`
  red pixels. The packaged Qt 5 run matched those measurements. This is an
  app-side camera-box overlay framebuffer guard only; it does not validate
  selection handles, tool cursor overlays, rulers, OS/menu interaction, or full
  overlay visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-safe-area-field-guide-qt6`. This app-side smoke
  creates a blank sandbox scene, disables the camera box, safe-area, and
  field-guide overlays, captures the `SceneViewer`, enables safe area and field
  guide through `MI_SafeArea` / `safeAreaToggle` and `MI_FieldGuide` /
  `fieldGuideToggle`, and requires `safeAreaProbe=ok`, `fieldGuideProbe=ok`,
  `viewerRenderProbe=ok`, the high-DPI framebuffer probes, changed pixels, red
  safe-area pixels, gray guide pixels, changed gray guide pixels, and
  before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, `112230` changed
  pixels, `5279` red pixels, `645999` gray pixels, and `106951` changed gray
  pixels. The packaged Qt 5 run matched those measurements. This is an
  app-side safe-area and field-guide overlay framebuffer guard only; it does not
  validate selection handles, tool cursor overlays, rulers/guides added through
  ruler UI, OS/menu interaction, custom safe-area presets or colors, custom
  field-guide settings, or full overlay visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-ruler-guide-qt6`. This app-side smoke creates a
  blank sandbox scene, seeds one horizontal and one vertical scene guide,
  disables camera, safe-area, field-guide, guide, and ruler overlays, enables
  the ruler, captures the `SceneViewer` with guides still disabled, enables the
  guide overlay through `MI_ViewGuide` / `viewGuideToggle`, and requires
  `rulerGuideProbe=ok`, `viewerRenderProbe=ok`, the high-DPI framebuffer probes,
  two visible ruler widgets, guide counts, changed pixels, changed neutral guide
  pixels, and before/after captures. The baseline capture is taken after the
  ruler is visible because toggling the ruler changes the viewer widget size.
  The packaged Qt 6 run reported logical viewer size `994x819`, DPR `2` /
  `2.00`, framebuffer `1988x1638`, `1707` changed pixels, `1707` changed
  neutral guide pixels, `551872` gray pixels, `0` changed gray pixels, and `0`
  red pixels. The packaged Qt 5 run matched those measurements. This is an
  app-side guide framebuffer and ruler-widget visibility guard only; it does not
  validate creating, moving, or deleting guides through the ruler UI, ruler tick
  rendering pixels, selection handles, tool cursor overlays, OS/menu
  interaction, custom guide positions or colors, or full overlay visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-overlay-qt6`. This app-side smoke
  creates a visible raster scene, captures a `T_Hand` baseline, switches the
  current tool to `T_Edit` on `Col1`, and requires `toolOverlayProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, changed pixels, visible
  red content pixels, and before/after captures. The packaged Qt 6 run reported
  logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  `2898` changed pixels, and `357248` red pixels. The packaged Qt 5 run matched
  those measurements. This covers the Animate/Edit tool-gadget overlay path
  through `SceneViewer::drawToolGadgets()` only; it does not validate selection
  handles, cursor feedback, interactive transform dragging, OS/menu
  interaction, or full overlay visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-drag-qt6`. This app-side smoke creates
  a visible raster scene, switches the current tool to `T_Edit` on `Col1`,
  captures the `SceneViewer` with the tool active, replays a direct Animate/Edit
  tool left-button drag from `0,0` to `144,-72`, and requires
  `toolTransformProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
  changed `TStageObject` placement, changed pixels, visible red content pixels,
  and before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, stage object X
  `0.0000 -> 2.7000`, stage object Y `0.0000 -> -1.3500`, `492731` changed
  pixels, and `356145` red pixels. The packaged Qt 5 run matched those
  measurements. This covers the direct Animate/Edit tool transform path only; it
  does not validate selection-handle hit-testing, cursor feedback, real
  OS-level or hardware input delivery, or full transform workflow visual parity.
  Undo/redo integration and modifier-key behavior are covered by the separate Qt
  mouse-event smokes below.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-mouse-events-qt6`. This app-side smoke
  creates a visible raster scene, switches the current tool to `T_Edit` on
  `Col1`, maps the same Animate/Edit transform path through `SceneViewer` Qt
  mouse press/move/release events using the viewer DPR, and requires
  `mouseEventProbe=ok`, `toolTransformProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, changed `TStageObject` placement, changed
  pixels, visible red content pixels, and before/after captures. The packaged
  Qt 6 run reported logical viewer size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, stage object X `0.0000 -> 1.3521`, stage object Y
  `0.0000 -> -0.6761`, `280162` changed pixels, and `357248` red pixels. The
  packaged Qt 5 run matched those measurements. This covers Qt widget event
  dispatch and high-DPI coordinate mapping for the Animate/Edit transform path;
  it does not validate real OS-level input delivery, selection-handle
  hit-testing, cursor feedback, or full transform workflow visual parity.
  Modifier-key behavior is covered by the separate Qt mouse-event modifier smoke
  below.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-undo-redo-qt6`. This app-side smoke
  creates the same visible raster scene, switches the current tool to `T_Edit` on
  `Col1`, maps the Animate/Edit transform path through `SceneViewer` Qt mouse
  press/move/release events using the viewer DPR, resets the undo stack before
  the drag, and requires `mouseEventProbe=ok`, `toolTransformProbe=ok`,
  `undoRedoProbe=ok`, `viewerRenderProbe=ok`, one undo history entry, exact
  undo restoration, redo restoration, high-DPI framebuffer probes, changed
  pixels, visible red content pixels, and before/after captures. The packaged
  Qt 6 run reported logical viewer size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, stage object X `0.0000 -> 1.3521 -> 0.0000 ->
  1.3521`, stage object Y `0.0000 -> -0.6761 -> 0.0000 -> -0.6761`, undo
  history `0 -> 1`, history indices `0/1/0/1`, `280162` changed pixels, and
  `357248` red pixels. The packaged Qt 5 run matched those measurements. This
  covers Qt widget event dispatch, high-DPI coordinate mapping, and undo/redo
  integration for the Animate/Edit transform path; it does not validate real
  OS-level input delivery, selection-handle hit-testing, full cursor
  artwork/hover variants, or full transform workflow visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-modifiers-qt6`. This app-side smoke
  creates the same visible raster scene, switches the current tool to `T_Edit`
  on `Col1`, maps normal, Alt, and Shift Animate/Edit drags through
  `SceneViewer` Qt mouse events using the viewer DPR, and requires
  `mouseEventProbe=ok`, `toolTransformProbe=ok`, `modifierProbe=ok`,
  `cursorPreciseProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
  visible red content pixels, and before/after captures. The packaged Qt 6 run
  reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, normal delta `1.3521,-0.6761`, Alt delta `0.1352,-0.0676`,
  Shift delta `1.3521,0.0000`, cursor precise state `false -> true`, `203603`
  changed pixels, and `357248` red pixels. The packaged Qt 5 run matched those
  measurements. This covers Qt modifier delivery, Alt precision transform,
  Shift dominant-axis constraint, and the Animate/Edit precise-cursor state for
  the Qt mouse-event transform path; it does not validate real OS-level input
  delivery, selection-handle hit-testing, full cursor artwork/hover variants, or
  full transform workflow visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes vector and full-color raster
  brush tool-input smokes through
  `mise run gui-smoke-viewer-vector-brush-qt6` and
  `mise run gui-smoke-viewer-raster-brush-qt6`. The app-side hook creates a
  sandbox level, selects `T_Brush`, replays a direct left-button stroke path
  through the tool, verifies that the vector stroke count or raster opaque/red
  pixel counts increase, captures the `SceneViewer` framebuffer before and
  after the stroke, and requires changed and red-dominant pixels. The raster
  brush hook was also run against the Qt 5 build to keep the smoke dual-lane
  compatible. These are narrow product-tool drawing guards; they do not yet
  validate OS-level mouse/tablet event delivery, pressure/tilt, high-DPI input
  mapping, timeline/UI onion workflow, tool overlays, FX preview, final render
  output, or visual parity against the Qt 5 baseline.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-raster-brush-mouse-events-qt6`. This smoke sends
  Qt mouse press/move/release events through `SceneViewer`, maps the shared
  world-space brush path into logical widget coordinates using the viewer DPR,
  verifies increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. The same mouse-event smoke was
  also run against the Qt 5 build. This narrows Qt widget event dispatch and
  high-DPI input coordinate coverage, but it still does not validate real
  OS-level CGEvent/System Events delivery, tablet input, timeline/UI onion
  workflow, overlays, FX preview, final render output, or visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-raster-brush-tablet-events-qt6`. This smoke sends
  synthetic Qt tablet press/move/release events through `SceneViewer`, maps the
  shared world-space brush path into logical widget coordinates using the
  viewer DPR, drives pressure from 0.35 to 0.85 with tilt `-18,12`, verifies
  increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. The same synthetic tablet
  smoke was also run against the Qt 5 build. This narrows Qt tablet-event
  dispatch, `TMouseEvent` tablet/pressure propagation, and high-DPI input
  coordinate coverage, but it still does not validate real tablet hardware,
  OS-level CGEvent/System Events delivery, platform driver pressure/tilt,
  timeline/UI onion workflow, overlays, FX preview, final render output, or
  visual parity.
- On 2026-05-30, this branch added the permission-sensitive
  `mise run gui-smoke-viewer-raster-brush-system-events-qt6` gate for real
  macOS OS-level input delivery. The app-side hook now prepares the same raster
  brush scene, publishes screen coordinates, waits for external input, and
  requires raster opaque/red pixel counts plus `SceneViewer` framebuffer pixels
  to change. The harness now launches this permission-sensitive smoke through
  LaunchServices with explicit smoke environment/log routing, preflights
  `CGPreflightPostEventAccess()`, attempts bundle/PID activation, and posts a
  prime click before the drag. In the current local session, this gate is still
  not green: `cgPostEventAccess=true`, but the target app remains inactive
  (`targetAppActiveAfter=false` and `targetAppActiveAfterPrimeClick=false`),
  raster pixels stay at zero, and the System Events click fallback still fails
  with macOS error `-25200`. Treat this as a current app-activation or OS-event
  delivery blocker, not as Qt 6 brush logic parity, because
  `mise run gui-smoke-viewer-raster-brush-mouse-events-qt6` still passes
  against the packaged Qt 6 app.
- The 2026-05-30 validation slice reran the Qt 6 app build, macOS Qt 6
  packaging, arm64 bundle check, raster viewer smoke, vector viewer smoke, and
  vector/raster direct-tool brush smokes. A later slice in the same day reran
  the Qt 6 arm64 bundle check, added the `SceneViewer` mouse-event raster brush
  smoke, reran the direct raster brush smoke, rebuilt the Qt 5 app target, and
  ran the same mouse-event raster brush smoke against the Qt 5 build. A further
  slice reran the Qt 6 app build, macOS Qt 6 packaging, arm64 bundle check,
  added the `SceneViewer` synthetic tablet-event raster brush smoke, and ran
  that tablet-event smoke against both the packaged Qt 6 app and the Qt 5 app
  build. A later script-binding slice rebuilt the Qt 6 app, refreshed the macOS
  Qt 6 package, reran the arm64 bundle check, made `Renderer.dumpCache()` write
  a portable cache-map diagnostic, reran the focused Renderer smoke, reran the
  aggregate Qt 6 script smokes in bounded and natural-exit modes, and rebuilt
  the Qt 5 app target. A subsequent Rasterizer script-binding slice rebuilt the
  Qt 6 app, refreshed the macOS Qt 6 package, reran the arm64 bundle check,
  ported `Rasterizer.rasterize()` full-color vector output through
  `TOfflineGL`, reran the focused Rasterizer and Rasterizer edge smokes, reran
  the aggregate Qt 6 script smokes in bounded and natural-exit modes, and
  rebuilt the Qt 5 app target. A subsequent Scene reload lifecycle slice
  rebuilt the Qt 6 app, refreshed the macOS Qt 6 package, reran the arm64 bundle
  check, invalidated scene-owned Qt 6 `Level` wrappers before `Scene.load()`
  replaces the native scene, added the focused Scene reload edge smoke, reran
  the aggregate Qt 6 script smokes in bounded and natural-exit modes, and
  rebuilt the Qt 5 app target. The current OS-input slice rebuilt the Qt 6 app,
  refreshed the macOS Qt 6 package, reran the arm64 bundle check, added the
  CGEvent/System Events raster brush smoke, recorded the local `-25200` System
  Events blocker, and reran the existing Qt mouse-event raster brush smoke
  successfully. A follow-up harness slice moved that OS-input smoke to a
  LaunchServices launch path, added CGEvent post-access and target-activation
  diagnostics, confirmed `cgPostEventAccess=true`, and narrowed the remaining
  failure to target activation/event delivery with unchanged raster pixels. The
  viewer high-DPI slice rebuilt both app targets, refreshed the macOS Qt 6 and
  Qt 5 packages, added viewer logical/device/framebuffer probes to the existing
  capture path, reran the Qt 6 arm64 bundle check, and verified that both
  packaged lanes pass the raster viewer-render smoke with matching DPR and
  framebuffer measurements. The follow-up viewer transform slice rebuilt the Qt
  6 app target, refreshed both macOS packages, added the `viewer-zoom-pan`
  smoke, reran the Qt 6 arm64 bundle check, and verified that both packaged
  lanes pass with matching DPR, transform, framebuffer, changed-pixel, and
  red-pixel measurements. The follow-up viewer onion-skin slice rebuilt both
  app targets, refreshed both macOS packages, added
  `viewer-onion-skin`, reran the Qt 6 arm64 bundle check, and verified that
  both packaged lanes pass with matching DPR, framebuffer, stage-player,
  current-frame red-pixel, and onion-pixel measurements. The follow-up camera
  overlay slice rebuilt both app targets, refreshed both macOS packages, added
  `viewer-camera-overlay`, reran the Qt 6 arm64 bundle check, and verified that
  both packaged lanes pass with matching DPR, framebuffer, changed-pixel, and
  red camera-box overlay measurements. The follow-up safe-area/field-guide
  overlay slice rebuilt both app targets, refreshed both macOS packages, added
  `viewer-safe-area-field-guide`, reran the Qt 6 arm64 bundle check, and
  verified that both packaged lanes pass with matching DPR, framebuffer,
  changed-pixel, red safe-area, gray guide, and changed-gray guide
  measurements. The follow-up ruler/guide overlay slice rebuilt both app
  targets, refreshed both macOS packages, added `viewer-ruler-guide`, reran the
  Qt 6 arm64 bundle check, and verified that both packaged lanes pass with
  matching DPR, framebuffer, changed-pixel, changed-neutral-guide-pixel,
  ruler-widget, and guide-count measurements.
- The follow-up Animate tool overlay slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-animate-tool-overlay`, reran the Qt 6 and
  Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, `T_Edit`/`Col1` state, changed-pixel, and red-pixel
  measurements.
- The follow-up Animate tool drag slice rebuilt both app targets, refreshed both
  macOS packages, added `viewer-animate-tool-drag`, reran the Qt 6 and Qt 5
  arm64 bundle checks, and verified that both packaged lanes pass with matching
  DPR, framebuffer, `T_Edit`/`Col1` state, `TStageObject` X/Y movement, changed
  pixels, and red-pixel measurements.
- The follow-up Animate tool mouse-event slice rebuilt both app targets,
  refreshed both macOS packages, added `viewer-animate-tool-mouse-events`, reran
  the Qt 6 and Qt 5 arm64 bundle checks, and verified that both packaged lanes
  pass with matching DPR, framebuffer, Qt mouse-event dispatch, `T_Edit`/`Col1`
  state, `TStageObject` X/Y movement, changed pixels, and red-pixel
  measurements.
- The follow-up Animate tool undo-redo slice rebuilt the Qt 6 and Qt 5 app
  targets as needed, refreshed both macOS packages, added
  `viewer-animate-tool-undo-redo`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer, Qt
  mouse-event dispatch, `T_Edit`/`Col1` state, `TStageObject` X/Y movement,
  undo/redo restoration, undo history indices, changed-pixel, and red-pixel
  measurements.
- The follow-up Animate tool modifier slice rebuilt the Qt 6 and Qt 5 app
  targets as needed, refreshed both macOS packages, added
  `viewer-animate-tool-modifiers`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer, Qt
  mouse-event dispatch, `T_Edit`/`Col1` state, normal/Alt/Shift `TStageObject`
  deltas, Alt precision ratio, Shift dominant-axis lock, precise-cursor state,
  changed-pixel, and red-pixel measurements.
- A non-rendering packaged Qt 6 high-DPI diagnostic smoke is now green through
  `mise run gui-smoke-highdpi-qt6`. It records main-window DPR, screen DPR,
  logical DPI, screen geometry, and the expected Qt 6 always-on high-DPI mode.
  This is a startup/window-state guard only; it does not validate viewer
  scaling, canvas pixels, tool overlays, or render output.
- A non-rendering packaged Qt 6 media-device enumeration smoke exists through
  `mise run gui-smoke-media-devices-qt6`. It records the Qt Multimedia API
  lane plus audio input, audio output, and camera counts/default names from the
  packaged app. This is an enumeration guard only; it does not validate audio
  playback, audio recording, camera permission prompts, preview frames, or
  still capture.
- A packaged Qt 6 camera-format enumeration smoke is green through
  `mise run gui-smoke-camera-formats-qt6`. It records the default camera
  format count, resolution bounds, frame-rate bounds, and compact pixel-format
  metadata through `QCameraDevice` without constructing or starting a camera
  capture session. The current local run found one video input, the FaceTime HD
  Camera as default, seven default-camera formats, 640x480 to 1920x1920
  resolution bounds, and 15-30 FPS frame-rate bounds. This proves packaged Qt 6
  camera metadata is reachable when a camera is present; it does not validate
  permission prompts, preview frames, still capture, hotplug behavior, or
  stop-motion UI workflows.
- A packaged Qt 6 audio-output backend smoke exists through
  `mise run gui-smoke-audio-output-qt6`. It starts the default Qt 6 audio
  output with a short silent buffer from inside the packaged app and records the
  selected format, backend state, and error status. This proves default output
  backend startup, not audible playback fidelity, recording, lip-sync preview,
  or microphone permission behavior.
- A packaged Qt 6 audio-input backend smoke is green through
  `mise run gui-smoke-audio-input-qt6`. It starts the default Qt 6 audio input
  from inside the packaged app, records the selected input and format, captures
  a short buffer, and verifies `QAudioSource` reports `NoError`. The current
  local run used the MacBook Pro Microphone at 48 kHz and captured 96,256
  bytes. This proves packaged default-input backend capture, not the Record
  Audio popup, WAV writing, permission UX, lip-sync timing, or noisy-room audio
  quality.
- A packaged Qt 6 audio-recording WAV smoke is green through
  `mise run gui-smoke-audio-recording-wav-qt6`. It records through the existing
  Record Audio WAV writer, saves a short WAV beside the GUI smoke status file,
  and reloads the result through `TSoundTrackReader`. The current local run
  used the MacBook Pro Microphone at 44.1 kHz, recorded 65,536 bytes, produced
  a 65,580-byte WAV, and reloaded 32,768 samples over 743 ms. This proves the
  packaged Qt 6 capture-to-WAV writer and sound I/O reload path, not the Record
  Audio popup button flow, Save and Insert column insertion, microphone
  permission UX, lip-sync timing, or capture quality.
- A packaged Qt 6 audio-playback WAV smoke is green through
  `mise run gui-smoke-audio-playback-wav-qt6`. It generates a low-volume PCM16
  WAV through `AudioWriterWAV`, reloads it through `TSoundTrackReader`, opens
  the default output through `TSoundOutputDevice`, and verifies playback starts
  and stops from inside the packaged app. The current local run used MacBook Pro
  Speakers at 44.1 kHz mono, wrote 264,600 bytes to a 264,644-byte WAV, and
  reloaded 132,300 samples over 3,000 ms. This proves the packaged Qt 6 product
  audio-output abstraction can start a generated WAV, not audible playback
  quality, chosen speaker route, flipbook/timeline playback, lip-sync preview,
  or Record Audio UI behavior.
- A first Qt 6 script smoke fixture exists at
  `toonz/sources/tests/scriptengine/basic.toonzscript` and is run by
  `mise run script-smoke-qt6`. It validates the `QJSEngine` bootstrap for
  `print`, `warning`, `run`, the legacy `ToonzVersion` global, and the legacy
  global `void` object returned by `print`/`warning` to suppress unwanted
  top-level evaluation-result output through the Qt 6 app bundle in headless
  `QCoreApplication` script mode. The `run()` check uses
  `toonz/sources/tests/scriptengine/run_child.toonzscript`, copied by the
  smoke harness into the isolated `stuff/library/scripts` tree, so it exercises
  the same library-script lookup path as the legacy Qt 5 engine.
- A `run()` error parity fixture exists at
  `toonz/sources/tests/scriptengine/run_errors.toonzscript` and is run by
  `mise run script-smoke-run-errors-qt6`. It validates that the Qt 6 bootstrap
  throws script-visible errors for missing arguments, extra arguments,
  non-file-path arguments, and missing script files instead of silently
  returning `undefined`.
- A second Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path.toonzscript` and is run by
  `mise run script-smoke-filepath-qt6`. It validates the first narrow
  `FilePath` compatibility facade on top of the Qt 6 `QJSEngine` backend.
- A third Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_basic.toonzscript` and is run by
  `mise run script-smoke-scene-qt6`. It validates the first narrow `Scene`
  compatibility facade: construction, `frameCount`, `columnCount`, `toString`,
  and column insert/delete against a real `ToonzScene` instance.
- A fourth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_basic.toonzscript` and is run by
  `mise run script-smoke-level-qt6`. It validates the first narrow `Level`
  compatibility facade: empty level handles, vector level creation through
  `Scene.newLevel()`, lookup/listing through `Scene`, name mutation,
  frame-count/frame-id access, and string/type reporting.
- A fifth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_basic.toonzscript` and is run by
  `mise run script-smoke-image-qt6`. It validates the first narrow `Image`
  compatibility facade: loading a bundled PNG, reporting type/size/DPI/string
  data, saving a raster image into the isolated script-smoke build directory,
  inserting that image into a raster `Level`, and reading the frame back by
  frame id and frame index.
- A sixth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_cells.toonzscript` and is run by
  `mise run script-smoke-scene-cells-qt6`. It validates the first narrow
  `Scene` cell API compatibility slice: setting cells by `Level` object,
  copying cells through the object returned by `Scene.getCell()`, setting cells
  by level name, clearing cells, and preserving xsheet frame/column counts.
- A Scene column mutation Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_columns.toonzscript` and is run by
  `mise run script-smoke-scene-columns-qt6`. It validates non-rendering xsheet
  column insertion/deletion behavior: populated cells shift across inserted
  columns, deleted columns remove their cells, remaining columns shift left, and
  frame/column counts remain consistent.
- A Scene cell frame-id type Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_cell_fids.toonzscript` and is run by
  `mise run script-smoke-scene-cell-fids-qt6`. It validates the legacy
  `Scene.getCell().fid` type contract: numeric frame ids return as numbers, and
  lettered frame ids return as strings.
- A Scene edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_edges.toonzscript` and is run by
  `mise run script-smoke-scene-edges-qt6`. It validates a non-rendering
  advanced scene API slice: Raster/ToonzRaster/Vector level creation, missing
  level lookup, duplicate level errors, bad level-type errors, bad cell object
  errors, missing-level cell assignment errors, rejection of non-string,
  non-`Level` cell level arguments, four-argument undefined level rejection,
  bad row/column errors, and load-level missing-file/duplicate-name errors.
- A Scene argument edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_argument_edges.toonzscript` and is
  run by `mise run script-smoke-scene-argument-edges-qt6`. It validates
  non-rendering Scene row/column argument rejection for insert/delete/get/set
  cell APIs, backend negative row/column errors, and bad frame-id rejection
  without entering scene icon, viewer, offscreen GL, or renderer paths.
- A Scene lifecycle edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_lifecycle_edges.toonzscript` and is
  run by `mise run script-smoke-scene-lifecycle-edges-qt6`. It validates
  cross-scene level isolation, scene disposal error behavior, and invalidated
  scene-owned level wrappers without entering viewer, offscreen GL, or renderer
  paths.
- A seventh Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_io.toonzscript` and is run by
  `mise run script-smoke-level-io-qt6`. It validates standalone empty
  `Level()` frame creation through `Level.setFrame()`, `Level.save()` to a real
  raster level path, and `Level.load()`/`new Level(path)` loading that saved
  level back through scene-decoded OpenToonz level I/O.
- An eighth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_load_level.toonzscript` and is run by
  `mise run script-smoke-scene-loadlevel-qt6`. It validates
  `Scene.loadLevel()` by loading a saved raster level into a `Scene`, checking
  the loaded level metadata, and assigning the loaded level to an xsheet cell.
- A Scene reload edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_reload_edges.toonzscript` and is run
  by `mise run script-smoke-scene-reload-edges-qt6`. It validates loading a
  saved scene into an existing `Scene` facade, replacing that scene with a
  second saved scene, restoring the first scene, checking the loaded level/cell
  state after each `Scene.load()`, and proving stale scene-owned `Level` wrappers
  are invalidated rather than kept live.
- A ninth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_builder.toonzscript` and is run by
  `mise run script-smoke-image-builder-qt6`. It validates the first narrow
  `Transform` and `ImageBuilder` compatibility slice: identity, translation,
  rotation, uniform scale, non-uniform scale, transform string reporting,
  translated raster composition, generated image access, image save and reload,
  clear/fill behavior, and typed Raster/ToonzRaster builder construction.
- An ImageBuilder edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_builder_edges.toonzscript` and is run
  by `mise run script-smoke-image-builder-edges-qt6`. It validates constructor
  argument-count/type/size errors, bad image type rejection, invalid fill color,
  empty-image add errors, non-`Transform` add argument rejection, ToonzRaster
  fill rejection, image type mismatch errors, and disposed builder errors.
- A Transform edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/transform_edges.toonzscript` and is run by
  `mise run script-smoke-transform-edges-qt6`. It validates finite-number
  argument rejection for `translate()`, `rotate()`, and `scale()`, plus
  disposed Transform object rejection without entering rendering paths.
- A tenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/toonz_raster_converter.toonzscript` and is
  run by `mise run script-smoke-toonz-raster-converter-qt6`. It validates the
  first narrow converter binding slice: `ToonzRasterConverter` construction,
  static raster-image conversion to a ToonzRaster `Image`, TLV save/reload, and
  existing compatibility helpers such as `toString()`, `foo()`, and
  `flatSource`.
- A companion Qt 6 converter fixture exists at
  `toonz/sources/tests/scriptengine/toonz_raster_converter_level.toonzscript`
  and is run by `mise run script-smoke-toonz-raster-converter-level-qt6`. It
  validates instance and static `ToonzRasterConverter.convert(Level)` behavior
  by converting a two-frame raster level to a ToonzRaster level, reading
  converted frames, saving the converted TLV, and reloading a frame from disk.
- A converter edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/toonz_raster_converter_edges.toonzscript`
  and is run by
  `mise run script-smoke-toonz-raster-converter-edges-qt6`. It validates
  legacy-style `ToonzRasterConverter.convert()` argument and type rejection for
  missing/extra arguments, non-image/non-level values, ToonzRaster and Vector
  images, empty Raster levels, and Vector levels.
- An eleventh Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/outline_vectorizer.toonzscript` and is run
  by `mise run script-smoke-outline-vectorizer-qt6`. It validates the first
  narrow vectorizer binding slice: `OutlineVectorizer` construction, property
  get/set behavior, raster-image vectorization to a Vector `Image`, PLI
  save/reload, and inserting the vectorized image into a vector `Level`.
- A twelfth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/centerline_vectorizer.toonzscript` and is
  run by `mise run script-smoke-centerline-vectorizer-qt6`. It validates the
  next narrow vectorizer binding slice: `CenterlineVectorizer` construction,
  property get/set behavior, raster-image vectorization to a Vector `Image`,
  PLI save/reload, and inserting the vectorized image into a vector `Level`.
- A vectorizer edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/vectorizer_edges.toonzscript` and is run by
  `mise run script-smoke-vectorizer-edges-qt6`. It validates legacy-style
  `OutlineVectorizer.vectorize()` and `CenterlineVectorizer.vectorize()`
  argument and type rejection for non-image/non-level values, Vector images,
  empty Raster levels, and Vector levels without entering renderer paths.
- A non-rendering binding lifecycle/property edge Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/binding_lifecycle_edges.toonzscript` and is
  run by `mise run script-smoke-binding-lifecycle-edges-qt6`. It validates
  property get/set roundtrips for `OutlineVectorizer`, `CenterlineVectorizer`,
  and `Rasterizer`, invalid `OutlineVectorizer.transparentColor` rejection, and
  disposed-object method/property rejection for `OutlineVectorizer`,
  `CenterlineVectorizer`, `Rasterizer`, and `ToonzRasterConverter`.
- A thirteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/rasterizer.toonzscript` and is run by
  `mise run script-smoke-rasterizer-qt6`. It validates the Rasterizer binding
  slice: construction, property get/set behavior, color-mapped vector-image
  rasterization to a ToonzRaster `Image`, TLV save/reload, insertion into a
  ToonzRaster `Level`, and full-color vector image/level rasterization through
  `TOfflineGL` to Raster `Image`/`Level` output. This fixture opts into
  `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` because the full-color path needs Qt's
  plugin/offscreen-surface path.
- A Rasterizer edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/rasterizer_edges.toonzscript` and is run by
  `mise run script-smoke-rasterizer-edges-qt6`. It validates legacy-style
  `Rasterizer.rasterize()` argument and type rejection for non-image/non-level
  values, Raster/ToonzRaster images, Raster/Empty levels, and bad full-color
  resolution/DPI rejection while keeping the color-mapped path green.
- A Qt 6 Renderer fixture exists at
  `toonz/sources/tests/scriptengine/renderer_basic.toonzscript` and is run by
  `mise run script-smoke-renderer-qt6`. It validates `Renderer` construction,
  read-only `id`, `frames`/`columns` array state, `toString()`,
  `renderFrame()`, `renderScene()`, and disposal. The fixture creates a
  scene-owned Raster level, renders one frame, saves/reloads the rendered PNG,
  renders a one-frame output level, and validates the Raster image/level shape.
  This fixture opts into `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` because renderer
  execution needs Qt's plugin/offscreen-surface path. It also validates that
  `dumpCache()` writes a portable cache-map diagnostic under the configured
  OpenToonz cache root and returns the generated `FilePath`.
- A Renderer edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/renderer_edges.toonzscript` and is run by
  `mise run script-smoke-renderer-edges-qt6`. It validates that
  `renderScene()` and `renderFrame()` reject missing/non-`Scene`/disposed scene
  arguments and bad frame values before reaching the Qt 6 renderer path.
- A fourteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/wrapper_id.toonzscript` and is run by
  `mise run script-smoke-wrapper-id-qt6`. It validates inherited Wrapper `id`
  parity for the non-rendering `QJSEngine` facades: `FilePath`, `Scene`,
  `Level`, `Image`, `Transform`, `ImageBuilder`, `ToonzRasterConverter`,
  `OutlineVectorizer`, `CenterlineVectorizer`, `Rasterizer`, and `Renderer`,
  including disposal-time `-1` ids for disposable facade objects.
- A fifteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_path.toonzscript` and is run by
  `mise run script-smoke-level-path-qt6`. It validates `Level.path` setter
  parity for both `FilePath` objects and strings by assigning a saved raster
  level path and reloading that level through the Qt 6 facade.
- A sixteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_transformers.toonzscript` and is run
  by `mise run script-smoke-level-transformers-qt6`. It validates level-wide
  non-rendering transformer parity for `OutlineVectorizer.vectorize(Level)`,
  `CenterlineVectorizer.vectorize(Level)`, and `Rasterizer.rasterize(Level)`.
- A seventeenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_edges.toonzscript` and is run by
  `mise run script-smoke-filepath-edges-qt6`. It validates FilePath edge-case
  parity for relative concatenation, absolute-path concat rejection,
  non-directory `files()` errors, and directory listing through the Qt 6
  facade.
- A FilePath metadata Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_metadata.toonzscript` and is run
  by `mise run script-smoke-filepath-metadata-qt6`. It validates
  `exists`, `isDirectory`, directory listing, and `lastModified` as a JS
  `Date` object for files, directories, and missing paths.
- A companion Qt 6 path-argument fixture exists at
  `toonz/sources/tests/scriptengine/path_arguments.toonzscript` and is run by
  `mise run script-smoke-path-arguments-qt6`. It validates legacy-style
  string/FilePath argument checking for `run()`, FilePath parent/concat
  helpers, Image/Level/Scene path methods, and constructor edge behavior that
  should not silently coerce arbitrary values through `String()`.
- An eighteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_io_types.toonzscript` and is run by
  `mise run script-smoke-level-io-types-qt6`. It validates standalone
  `Level.save()`/`Level.load()` coverage beyond raster image sequences by
  saving and reloading a Vector level with frame-image access and a ToonzRaster
  level with type/frame metadata plus `Level.getFrame()` and
  `Level.getFrameByIndex()` access after reloading the saved ToonzRaster
  level.
- A nineteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_save_reopen.toonzscript` and is run
  by `mise run script-smoke-scene-save-reopen-qt6`. It validates a headless
  `Scene.save()`/`new Scene(path)` data roundtrip for a scene that references a
  saved raster level. The Qt 6 script save path deliberately skips scene-icon
  generation in this headless mode, so this fixture does not claim scene icon,
  viewer, offscreen GL, or renderer parity.
- A twentieth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_save_icon.toonzscript` and is run by
  `mise run script-smoke-scene-save-icon-qt6`. It validates the
  `QApplication` script path for `Scene.save()` scene-icon generation by saving
  a scene, checking the generated `sceneIcons` PNG, and loading that icon back
  through the Qt 6 `Image` facade. This is a narrow offscreen-rendering
  boundary check; it does not claim scene-icon visual parity, viewer parity, or
  broader render-output parity.
- A twenty-first Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_frameids.toonzscript` and is run by
  `mise run script-smoke-scene-frameids-qt6`. It validates lettered `TFrameId`
  handling across `Level.setFrame()`, `Level.getFrame()`,
  `Level.getFrameByIndex()`, `Scene.setCell()`, `Scene.getCell()`, and a
  headless `Scene.save()`/`new Scene(path)` roundtrip without entering scene
  icon, viewer, offscreen GL, or renderer paths.
- A Level edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_edges.toonzscript` and is run by
  `mise run script-smoke-level-edges-qt6`. It validates empty-level frame
  access errors, empty-level save errors, missing-frame `undefined` behavior,
  out-of-range and non-number frame-index errors, bad frame-id rejection,
  empty-level name setter no-op parity, empty-level path `undefined` parity,
  level/image type mismatch rejection, incompatible save rejection, and
  missing-level load errors.
- An Image edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_edges.toonzscript` and is run by
  `mise run script-smoke-image-edges-qt6`. It validates empty image metadata
  and save errors, constructor argument-count errors, non-path argument
  rejection, missing-image load errors that clear the image handle, incompatible
  raster/ToonzRaster save rejection, and unrecognized output type errors.
- The script smoke can run in bounded mode or natural-exit mode. The basic,
  `run()` error, FilePath, FilePath edges, FilePath metadata, path-argument,
  Scene, Scene cells, Scene columns,
  Scene cell frame-id type, Scene edges, Scene argument edges,
  Scene lifecycle edges, Scene load-level, Scene save/reopen, Scene reload edge,
  Scene save-icon, Scene frame-id, Level, Level I/O,
  Level edge-case, Level I/O types, Image, Image edge-case, ImageBuilder,
  ImageBuilder edge-case, Transform edge-case, ToonzRasterConverter,
  ToonzRasterConverter level conversion, ToonzRasterConverter edge-case,
  OutlineVectorizer,
  CenterlineVectorizer, vectorizer edge-case, binding lifecycle edge,
  Rasterizer including full-color output, Renderer, Renderer edge-case, Wrapper
  id, Rasterizer edge-case, Level path, and level transformer fixtures pass in
  both modes for the current Qt 6 app bundle.
  The default script entry path now intentionally exits before the normal macOS
  `QApplication`/plugin/main-window startup and uses `stuff/cache` instead of
  the platform cache root so sandboxed CLI smokes do not write into a user
  Library cache. Renderer, Rasterizer full-color, and scene-icon smokes opt back
  into `QApplication` with `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1`; that path
  restores the launch working directory before script execution so existing
  relative fixture paths keep the same meaning after macOS bundle startup
  temporarily changes directories.
  The script-smoke harness also creates an isolated
  `SystemVar.ini`, passes explicit Toonz path qualifiers, and launches from the
  smoke root with a `toonz` symlink back to the checkout so profile/config/cache
  writes stay under `toonz/build/qt6-script-smoke` instead of creating repo-root
  `profiles` files. Unix `TEnv` startup now honors explicit process
  environment values before the legacy `SystemVar.ini` warning fallback, and
  the harness exports the same isolated Toonz paths before launch.
- `mise run script-smokes-qt6` runs every current Qt 6 script fixture
  in bounded mode, and `mise run script-smokes-natural-exit-qt6` runs the same
  fixture set while requiring the app process to exit naturally. Use the
  aggregate tasks as the default script-parity regression gate after adding a
  new fixture, while keeping individual tasks available for focused triage.
- After every macOS Qt 6 relink, `mise run package-macos-qt6` is still required
  before GUI smokes. The current package pass is very noisy and slow because
  `macdeployqt` rewrites a large transitive Qt/OpenCV/FFmpeg dependency set
  before re-signing the bundle.
- This is not product-ready Qt 6 support. Narrow packaged raster/vector viewer
  framebuffer smokes, a direct `SceneViewer` zoom/pan transform smoke,
  app-side onion-skin, camera-box, safe-area/field-guide, ruler/guide, and
  Animate/Edit tool overlay, direct-drag, Qt mouse-event transform, and
  undo-redo/modifier-key transform smokes, vector/full-color raster direct-tool
  brush smokes, and a `SceneViewer` Qt mouse-event plus synthetic tablet-event
  raster brush smoke exist, but real OS-level/hardware tablet input, full
  drawing workflows, timeline/UI onion workflow, selection-handle hit-testing
  and overlays, real OS-level transform dragging, broader overlay and high-DPI
  visual behavior, broader rendering, non-macOS packaging, scripting object
  binding parity, and hardware camera/audio smoke remain open.

## Next Implementation Slice

Do not redo the first runway slice unless the current branch has been discarded.
The next slice should make the Qt 6 app useful enough to run and diagnose:

1. Continue packaged Qt 6 interactive GUI smoke beyond the now-green scripted
   startup/create/open/xsheet-state, raster/vector viewer-framebuffer, direct
   viewer zoom/pan transform, app-side onion-skin, camera-box,
   safe-area/field-guide, ruler/guide, Animate/Edit tool overlay framebuffer,
   direct Animate/Edit tool transform-drag, and Qt mouse-event Animate/Edit
   transform-drag plus undo-redo and modifier-key behavior, vector/raster brush
   tool-input, Qt mouse-event, and synthetic Qt tablet-event paths. A real
   macOS
   CGEvent/System Events raster-brush gate now exists, but
   the current local run is blocked by app activation or OS input delivery:
   CGEvent post access is trusted, yet the smoke-launched target never becomes
   active, System Events still reports `-25200`, and raster pixels remain
   unchanged. Resolve or rerun that permissioned path before treating OS-level
   mouse delivery as covered. Then continue into hardware tablet pressure/tilt,
   timeline/UI onion workflow, selection-handle hit-testing and overlays, real
   OS-level transform dragging, interactive ruler-guide workflows, broader
   high-DPI input and viewer/rendering behavior, and OpenGL fallback warning
   triage.
2. Keep the app-side GUI smoke status hook narrow and test-only. System Events
   remains useful as a fallback/manual diagnostic, but do not make Qt 6
   create/open validation depend on macOS accessibility window discovery unless
   that path becomes repeatable again.
3. Extend the script fixture set beyond the current `run()` error, `FilePath`,
   FilePath-edge, FilePath metadata, path-argument, `Scene`, Scene edge-case,
   `Level`, `Level.path`,
   Level edge-case, level-wide transformer, Level I/O types including
   ToonzRaster reload frame access, scene data save/reopen, Scene reload edge,
   Scene save-icon,
   Scene frame-id
   handling, Scene cell frame-id type, Scene argument edge cases,
   Scene lifecycle edge cases, `Image`, Image edge-case,
   `ImageBuilder`, ImageBuilder edge-case, Transform edge-case,
   ToonzRasterConverter, ToonzRasterConverter level conversion,
   ToonzRasterConverter edge-case,
   OutlineVectorizer/CenterlineVectorizer/Rasterizer including full-color
   Rasterizer output, Renderer
   renderFrame/renderScene/dumpCache, Renderer edge-case, Wrapper id
   compatibility, and binding lifecycle/property edge slices into the next
   `QJSEngine` object-binding group, likely remaining advanced scene APIs,
   broader scene-icon/offscreen visual parity, or another helper subset, rather
   than attempting a full script API rewrite in one change.
4. Run focused product-level audio playback/recording and camera smoke tests on
   real hardware. The current audio-input, audio-output, audio-recording WAV,
   audio-playback WAV, and camera-format smokes are backend, WAV-writer, sound
   I/O reload, product-output-start, and enumeration guards; still add or run
   Record Audio UI Save and Insert column insertion, audible playback
   confirmation, lip-sync preview/timing, camera preview, and still-capture
   checks before calling multimedia product-ready.
5. Keep the basic, `run()` error, FilePath, FilePath-edge, FilePath metadata,
   path-argument, Scene, Scene cells, Scene columns,
   Scene cell frame-id type, Scene edges, Scene argument edges,
   Scene lifecycle edges, Scene load-level, Scene save/reopen,
   Scene reload edge,
   Scene save-icon, Scene frame-id,
   Level,
   Level edge-case, Level I/O, Level path,
   level transformer, Image,
   Image edge-case, ImageBuilder, ImageBuilder edge-case, Transform edge-case,
   Level I/O types,
   ToonzRasterConverter, ToonzRasterConverter level conversion,
   ToonzRasterConverter edge-case, OutlineVectorizer, CenterlineVectorizer,
   vectorizer edge-case, binding lifecycle edge, Rasterizer including
   full-color output, Rasterizer edge-case, Renderer including `dumpCache()`,
   Renderer edge-case, and Wrapper
   id script fixtures green in both bounded and natural-exit smoke modes while
   adding the next object-binding fixture.
6. Bring Qt 6 viewer/rendering work forward directly. Keep changes narrow and
   validated, but do not wait on Metal for viewer redraw, offscreen rendering,
   scene icons, render output, or visual-parity fixes.

## Required Technical Direction

Use version-aware Qt targets for this repository rather than relying broadly on
versionless `Qt::` aliases. The current tree contains many libraries and
platform branches, so explicit Qt major selection is easier to audit.

The Qt 6 lane should expect these components:

- `Core`
- `Gui`
- `Network`
- `OpenGL`
- `OpenGLWidgets`
- `Svg`
- `Xml`
- `Widgets`
- `PrintSupport`
- `LinguistTools`
- `Multimedia`
- `MultimediaWidgets`
- `SerialPort`
- `UiTools`
- `Core5Compat` as a temporary bridge
- `Qml` for the future `QJSEngine` script runtime

Do not treat `Qt5::Script` as mechanically portable. The Qt Script replacement
must be designed as a separate workstream using a project-owned scripting
facade and a `QJSEngine` backend.

## Follow-On Workstreams

Proceed in this order unless the live runtime or compile errors force a narrower
dependency order:

1. Keep Qt 5 as the default lane and rerun a focused Qt 5 build after every
   shared-source fix.
2. Launch the Qt 6 app from the build tree and make resource/plugin/rpath fixes
   needed for a basic startup smoke.
3. Add script compatibility fixtures and port OpenToonz object bindings behind
   the project-owned scripting facade.
4. Expand the `ttextcodec.h` adapter only where legacy encodings still require
   exact behavior, keep Core5Compat scoped to adapter-owning targets, and remove
   the targeted `image`/`toonzlib`/`OpenToonz` links once equivalent Qt 6 APIs
   cover the remaining call sites.
5. Finish audio recording and playback validation on real devices. The
   packaged WAV-writer smoke is green, but Record Audio UI insertion, audible
   playback, and lip-sync workflows still need product-level validation.
6. Finish stop-motion camera preview and still capture validation.
7. Continue narrow compile-frontier work, preserving both Qt 5 and Qt 6
   validation.
8. Finish Qt 6 viewer/rendering parity first, then revisit the Metal migration
   as a separate follow-up using the Qt 6-stabilized rendering behavior as the
   baseline.
9. Add Qt 6 packaging lanes for macOS, Linux, and Windows.
10. Remove or reduce transitional `Core5Compat` usage after equivalent Qt 6
    APIs are in place.

## Validation Expectations

For the first slice, run:

```sh
mise run configure
mise run build
mise run check
mise run doctor-qt6
mise run configure-qt6
git diff --check
```

If `mise run build` is too expensive for the current turn, run the most focused
build target available and state the limitation clearly.

For later slices, add the relevant checks:

```sh
mise run build-qt6
cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz --parallel
cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
mise run package-macos-qt6
mise run check-macos-arm64-qt6
mise run check-textcodec
mise run check-textcodec-qt6
mise run gui-smoke-qt6
mise run gui-smoke-scene-create-qt6
mise run gui-smoke-highdpi-qt6
mise run gui-smoke-media-devices-qt6
mise run gui-smoke-camera-formats-qt6
mise run gui-smoke-audio-input-qt6
mise run gui-smoke-audio-output-qt6
mise run gui-smoke-audio-recording-wav-qt6
mise run gui-smoke-audio-playback-wav-qt6
mise run gui-smoke-xsheet-scrub-qt6
mise run gui-smoke-viewer-render-qt6
mise run gui-smoke-viewer-vector-render-qt6
mise run gui-smoke-viewer-zoom-pan-qt6
mise run gui-smoke-viewer-onion-skin-qt6
mise run gui-smoke-viewer-camera-overlay-qt6
mise run gui-smoke-viewer-safe-area-field-guide-qt6
mise run gui-smoke-viewer-ruler-guide-qt6
mise run gui-smoke-viewer-animate-tool-overlay-qt6
mise run gui-smoke-viewer-animate-tool-drag-qt6
mise run gui-smoke-viewer-animate-tool-mouse-events-qt6
mise run gui-smoke-viewer-animate-tool-undo-redo-qt6
mise run gui-smoke-viewer-animate-tool-modifiers-qt6
mise run gui-smoke-viewer-vector-brush-qt6
mise run gui-smoke-viewer-raster-brush-qt6
mise run gui-smoke-viewer-raster-brush-mouse-events-qt6
mise run gui-smoke-viewer-raster-brush-tablet-events-qt6
mise run gui-smoke-viewer-raster-brush-system-events-qt6
OPENTOONZ_GUI_SMOKE_FILE=<path-to-scene.tnz> bash scripts/qt6/run-gui-smoke.sh
mise run script-smoke-qt6
mise run script-smoke-run-errors-qt6
mise run script-smoke-filepath-qt6
mise run script-smoke-filepath-edges-qt6
mise run script-smoke-filepath-metadata-qt6
mise run script-smoke-path-arguments-qt6
mise run script-smoke-scene-qt6
mise run script-smoke-scene-cells-qt6
mise run script-smoke-scene-columns-qt6
mise run script-smoke-scene-cell-fids-qt6
mise run script-smoke-scene-edges-qt6
mise run script-smoke-scene-argument-edges-qt6
mise run script-smoke-scene-lifecycle-edges-qt6
mise run script-smoke-scene-loadlevel-qt6
mise run script-smoke-scene-save-reopen-qt6
mise run script-smoke-scene-reload-edges-qt6
mise run script-smoke-scene-save-icon-qt6
mise run script-smoke-scene-frameids-qt6
mise run script-smoke-level-qt6
mise run script-smoke-level-edges-qt6
mise run script-smoke-level-io-qt6
mise run script-smoke-level-io-types-qt6
mise run script-smoke-level-path-qt6
mise run script-smoke-image-qt6
mise run script-smoke-image-edges-qt6
mise run script-smoke-image-builder-qt6
mise run script-smoke-image-builder-edges-qt6
mise run script-smoke-transform-edges-qt6
mise run script-smoke-toonz-raster-converter-qt6
mise run script-smoke-toonz-raster-converter-level-qt6
mise run script-smoke-toonz-raster-converter-edges-qt6
mise run script-smoke-outline-vectorizer-qt6
mise run script-smoke-centerline-vectorizer-qt6
mise run script-smoke-vectorizer-edges-qt6
mise run script-smoke-binding-lifecycle-edges-qt6
mise run script-smoke-rasterizer-qt6
mise run script-smoke-rasterizer-edges-qt6
mise run script-smoke-renderer-qt6
mise run script-smoke-renderer-edges-qt6
mise run script-smoke-level-transformers-qt6
mise run script-smoke-wrapper-id-qt6
mise run script-smokes-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-run-errors-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-metadata-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-path-arguments-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-cells-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-columns-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-cell-fids-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-argument-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-loadlevel-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-save-reopen-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-reload-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-save-icon-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-frameids-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-io-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-io-types-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-path-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-builder-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-builder-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-transform-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-level-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-outline-vectorizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-centerline-vectorizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-vectorizer-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-binding-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-rasterizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-rasterizer-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-transformers-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-wrapper-id-qt6
mise run script-smokes-natural-exit-qt6
```

Manual smoke validation is required before calling the Qt 6 port product-ready:

- launch OpenToonz
- open an existing scene
- draw raster and vector strokes
- scrub xsheet/timeline
- validate viewer redraw and high-DPI visual behavior
- render a preview and a final frame
- run script fixtures
- record and play audio
- use camera preview and still capture
- test tablet/stylus input on Windows
- launch packaged artifacts, not only build-tree binaries

## Reporting Requirements

Every handoff must include:

- files changed
- commands run
- commands not run and why
- current Qt 5 status
- current Qt 6 status
- remaining known blockers
- whether any touched files affect viewer/rendering parity and how they were
  validated in both Qt lanes

Do not claim Qt 6 support until the Qt 6 lane configures, compiles, packages,
launches, and passes the relevant GUI/hardware smoke checks.
