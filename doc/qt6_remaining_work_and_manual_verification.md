# Qt 6 Remaining Work And Manual Verification Guide

Prepared: June 2, 2026

This document consolidates the current Qt 5 to Qt 6 port status into a working
implementation checklist and a manual verification guide. It is intentionally
separate from `doc/qt6_migration_study.md` and
`doc/qt6_migration_goal_prompt.md`: those files hold the detailed study and
goal prompt, while this file is meant to be used during implementation and QA.

## Scope And Current Baseline

The active strategy is to finish the Qt 6 port before revisiting the separate
Metal migration. The Qt 5 lane must remain buildable until Qt 6 has reached
release-quality behavior and maintainers intentionally retire Qt 5 support.

Current useful baseline:

- The default Qt 5 lane still exists.
- A separate Qt 6 lane exists through CMake, Nix, CMake presets, and mise.
- The macOS Qt 6 app bundle builds, packages, launches through focused smokes,
  and passes the current arm64 Mach-O bundle check.
- The Qt 6 branch has substantial automated smoke coverage for startup, scene
  create/open, high DPI, viewer rendering, final render, FX Preview, tools,
  overlays, Qt Multimedia backend probes, and the current QJSEngine script
  compatibility slices.
- The automated smoke suite is still not a substitute for manual product
  parity. Most remaining risk is in real workflows, real OS input delivery,
  production scenes, cross-platform packaging, and release polish.

## Implementation Work Remaining

### 1. Release Criteria And Sequencing

Keep these project-level rules in force:

- Qt 6 is the active priority.
- The Metal migration is deferred until Qt 6 reaches visual and workflow parity
  with the current OpenToonz behavior.
- Qt 5 must stay green until the project intentionally removes it.
- Automated smoke success must not be treated as product-ready visual parity.
- Hardware-dependent checks should remain explicit gaps when no hardware is
  available.

Before the Qt 6 port can be considered complete, the project needs a written
release signoff that covers:

- Qt 5 baseline comparison.
- macOS, Windows, and Linux packaging.
- Viewer, drawing, rendering, preview, and final-render parity.
- Scripting compatibility.
- Camera, audio, and stop-motion workflows.
- Tablet/stylus behavior on at least the platforms the release supports.
- Translation/resource behavior when `WITH_TRANSLATION` is enabled.
- Known unsupported behavior, if any.

### 2. Build, Dependencies, And Compatibility Debt

Already covered:

- `OPENTOONZ_QT_MAJOR` and Qt-version-aware CMake selection exist.
- Qt target, MOC, resource, and translation command selection is centralized.
- Qt 6 builds link with `OpenGLWidgets`, scoped `Core5Compat`, and the current
  `QJSEngine`/Qml dependency where needed.
- Direct `QRegExp` usage is guarded against by `mise run check-no-qregexp`.
- Direct `QTextCodec` usage is isolated behind the legacy text-codec adapter
  and guarded by `mise run check-core5compat-scope`.
- An isolated Qt 6 translation configure lane exists through
  `nix-qt6-translation-check` and `mise run configure-qt6-translations`.
  This verifies Qt 6 translation target generation without perturbing the
  default Qt 5 or normal Qt 6 build directories.
- Direct checkbox state-change connects in `toonz`, `toonzqt`, and shared
  headers are centralized behind `QtCompat::connectCheckStateChanged`, with a
  Qt 6.7+ `checkStateChanged` path and a Qt 5 `stateChanged` fallback.
- Selected `QMouseEvent` and `QDropEvent`-derived coordinate users are
  centralized behind `QtCompat` helpers so the Qt 6 lane uses
  `position()`/`globalPosition()` APIs while the Qt 5 lane keeps its existing
  integer-position behavior.
- Synthetic `QMouseEvent` construction for Xsheet auto-pan and fake
  context-menu releases is centralized behind `QtCompat::makeMouseEvent()` with
  explicit local/global positions. The stale disabled spreadsheet auto-pan
  sample was removed so direct constructor scans report only the helper.
- `PlaneViewer` mouse press/move handling now uses
  `QtCompat::mouseEventPosition()` instead of deprecated `QMouseEvent::x()` /
  `y()` accessors.
- Shared `toonzqt` numeric fields, paired numeric range fields, spectrum key
  editing, color sample fields, swatch point editing/panning, scroll widgets,
  flip-console slider/split-button controls, Function Tree channel middle-drag
  initiation, mini-toolbar dragging, and dock
  hover/drag/resize/separator/drop-placeholder paths now use `QtCompat`
  event-position helpers instead of direct Qt 5-era event coordinate accessors.
- Shared `toonzqt` tree item hit testing, drag dispatch, context-menu
  placement, and tone-curve control-point editing now use `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` /
  `globalPos()` calls.
- `SceneViewer` mouse/tablet event initialization, tablet context-menu
  delivery, tablet hover-edge handling, and mouse double-click mapping now use
  `QtCompat` mouse/tablet position helpers instead of direct Qt 5-era event
  coordinate accessors.
- `FunctionPanel` and its graph drag tools now use `QtCompat` mouse-position
  helpers for graph hit testing, panning, zooming, keyframe dragging, handle
  dragging, rectangular selection, stretch operations, and context-menu
  placement.
- `DvTextEdit` mini-toolbar font-size population and text-family formatting now
  use Qt 5/Qt 6-compatible APIs instead of Qt 6-deprecated `QFontDatabase`
  instance construction and `QTextCharFormat::setFontFamily()`.
- Configure Shortcuts multi-key conflict checking now uses
  `QKeyCombination::toCombined()` on Qt 6 instead of the deprecated implicit
  `QKeyCombination` to `int` conversion, with the existing integer key-sequence
  path preserved on Qt 5.
- Separate Colors color-string defaults and settings restoration parse stored
  strings through `QColor(QString)` instead of calling Qt 6-deprecated
  `QColor::setNamedColor()`.
- Viewer touch gesture paths are centralized behind `QtCompat` helpers so the
  Qt 6 lane uses `QTouchEvent::points()` and `QEventPoint` positions while the
  Qt 5 lane keeps the existing `touchPoints()` / `QTouchEvent::TouchPoint`
  behavior.
- Deprecated `QVariant::canConvert(QVariant::...)` overloads are no longer
  present in the current app/UI/header tree. `DvDirTreeView` and the remaining
  settings restore paths use templated `canConvert<T>()` checks instead.

Still needed:

- Keep the Qt 5 build and Qt 6 build both passing after every shared source
  change.
- Reduce and eventually remove `Core5Compat` once the legacy text-codec adapter
  no longer needs it.
- Keep direct `QTextCodec`, `QRegExp`, and `QRegExpValidator` out of feature
  code.
- Build and inspect generated `.qm` files in a release workflow before treating
  translation generation as fully release-ready.
- Continue reducing the Qt 6 deprecation warning frontier, including
  remaining legacy event-coordinate accessors outside the completed
  SceneViewer, Function Panel, and shared-widget slices and the broader OpenGL
  warning field.
- Add or wire CI coverage for the Qt 6 lane on macOS, Linux, and Windows.
- Make any remaining Qt 5 specific setup in documentation conditional or
  clearly marked as Qt 5 only.

### 3. Qt Script To QJSEngine Productization

Already covered:

- Qt 6 has a QJSEngine-based runtime shell for the app's script system.
- The smoke suite covers the current facades for file/path, scene, level,
  image, image builder, rasterizer, vectorizers, renderer, wrapper id, binding
  lifecycle, and several edge cases.
- Aggregate script smoke tasks exist in both bounded and natural-exit modes.

Still needed:

- Finish porting the remaining object binding groups that are still partial or
  Qt 5 oriented.
- Expand compatibility fixtures with real user scripts, not just synthetic
  coverage.
- Verify legacy script-visible behavior for object lifetime, invalid argument
  handling, property conversion, arrays, frame ids, paths, scene mutation, and
  renderer output.
- Decide the replacement story for `QScriptEngineDebugger`, which has no direct
  Qt 6 equivalent.
- Confirm Script Console behavior manually, including `print`, `warning`,
  `run`, `view(Image)`, `view(Level)`, error display, and cleanup.
- Remove `Qt5::Script` from all Qt 6 production targets.
- Keep the Qt 5 script backend or retire it only after the compatibility suite
  proves that Qt 6 behavior is acceptable.

### 4. Qt Multimedia, Audio, Camera, And Stop-Motion

Already covered:

- Qt 6 API migration work exists for audio playback, audio recording, active
  pencil-test camera paths, and stop-motion camera enumeration.
- Packaged Qt 6 smokes cover device enumeration, camera-format metadata,
  default audio input, default audio output, WAV recording/reload, and
  generated-WAV playback startup.

Still needed:

- Run product-level audio recording through the Record Audio popup.
- Verify microphone permission prompts and failure behavior.
- Verify audible playback quality and output-device routing.
- Verify timeline/flipbook/lip-sync playback timing with real scenes.
- Verify camera permission prompts.
- Verify live camera preview frames.
- Verify still capture and frame insertion.
- Verify hotplug behavior for camera and audio devices.
- Verify stop-motion UI workflows end to end.
- Replace any remaining Qt 5 multimedia classes such as video-surface handling
  with Qt 6 equivalents, including `QVideoSink` where appropriate.

### 5. Viewer, OpenGL, Rendering, And Visual Parity

Already covered by focused smokes:

- Packaged startup and high-DPI diagnostics.
- Raster and vector viewer framebuffer rendering.
- Direct viewer zoom/pan transform.
- Previewer render-cache output.
- FX Preview manager/cache output.
- FX Preview sub-camera cache output.
- FX Preview flipbook frame switching, clone, freeze, and unfreeze.
- FX Preview full-frame and sub-camera saved-frame export.
- Final-render PNG output for basic raster, background, sequence, composite,
  vector, Toonz Raster, and a basic FX path.
- Several offscreen/script renderer and rasterizer slices.

Still needed:

- Manual File > Preview workflow validation.
- Manual schematic FX Preview command validation.
- Live FX Preview playback and timer behavior.
- Live sub-camera dragging and visual framing.
- Manual preview-save popup, overwrite, warning, and cancellation behavior.
- Production FX-heavy preview scenes.
- Broader final-render parity against Qt 5 using real scenes.
- Broader vector rendering parity.
- High-DPI visual behavior beyond the current framebuffer probes.
- OpenGL fallback message triage.
- Blank-viewer and stale-frame regression checks on realistic scenes.
- Qt 5 versus Qt 6 screenshot or pixel comparison for representative workflows.

### 6. Drawing, Tools, Input, And Tablet Behavior

Already covered by focused smokes:

- Vector brush direct tool-input path.
- Raster brush direct tool-input path.
- Raster brush Qt mouse-event path.
- Raster brush synthetic Qt tablet-event path.
- Animate/Edit overlay, direct drag, Qt mouse-event drag, undo/redo,
  modifiers, handles, handle variants, axis drags, and cursor mode signatures.
- Selection tool vector and raster handle/mode paths.

Still needed:

- Real OS-level mouse event delivery. The current macOS CGEvent/System Events
  raster-brush gate exists, but the current local run was blocked by app
  activation or OS input delivery.
- Real OS-level cursor delivery.
- Full manual transform workflow parity.
- Multi-object and multi-frame selection workflows.
- Remaining cursor artwork outside the checked Animate/Edit cursor set.
- Full drawing workflow parity, including repeated edits, undo/redo, save,
  reopen, and redraw behavior.
- Real tablet/stylus hardware pressure, tilt, hover, eraser, buttons, and
  device switching.

Tablet note:

The synthetic tablet smoke is useful for Qt event dispatch and
`TMouseEvent` pressure propagation. It is not proof of real hardware behavior.
If no tablet is available, record hardware tablet validation as deferred rather
than blocking the Qt 6 port. Only replace that gap with a simulator result if
the simulator exercises the OS and Qt platform tablet path credibly.

### 7. Timeline, Xsheet, Onion Skin, Guides, Rulers, And Overlays

Already covered by focused smokes:

- Xsheet state scrub.
- Onion-skin framebuffer behavior.
- Row-area onion marker UI, drag ranges, fixed marker drag, context-menu
  commands, custom colors, orientations, and Shift and Trace.
- Camera-box overlay.
- Safe-area and field-guide overlays, presets, colors, custom files, and field
  guide settings.
- Ruler/guide visibility, event behavior, variants, guide-line rendering,
  styles, and ruler ticks.

Still needed:

- Broader manual timeline/xsheet interaction.
- Onion workflows beyond the app-side row-area marker, fixed marker, context,
  color, orientation, and Shift and Trace coverage.
- Real OS-level menu and input delivery for timeline/overlay workflows.
- Remaining manual ruler/guide variants beyond app-side create, move, delete,
  and drag-hide coverage.
- Guide-line visual variants beyond the current custom-position dashed-line
  coverage.
- Overlay coexistence under real rooms, panels, DPI settings, and stylesheets.

### 8. Packaging, Deployment, And CI

macOS already covered:

- The Qt 6 app bundle packages.
- Qt 6 multimedia and SVG/icon plugins are included.
- Qt plugin framework references are rewritten into the bundle.
- The current arm64 bundle check passes.

macOS still needed:

- Re-run packaging after every GUI-affecting relink before manual testing.
- Reduce repeated `macdeployqt` install-name churn if it keeps slowing the
  loop.
- Re-check release signing, notarization, helper executables, and copied
  runtime data.
- Confirm packaged Qt plugins and frameworks are loaded from the bundle.

Windows still needed:

- Add a Qt 6 Windows CI lane.
- Replace the custom Qt 5.15.2 WinTab package strategy with a Qt 6 strategy.
- Re-audit `windeployqt` arguments and plugin output.
- Validate OpenGL, tablet/stylus, camera, audio, and file associations.

Linux still needed:

- Add an Ubuntu Qt 6 CI lane.
- Audit Qt 6 package names and plugin package coverage.
- Validate X11 and Wayland behavior separately.
- Re-test deployment tooling, runtime library paths, multimedia backends, and
  OpenGL behavior.

### 9. QA And Release Stabilization

Still needed:

- Define the release candidate build matrix.
- Define the Qt 5 baseline version and test data used for parity comparison.
- Collect representative production scenes, including raster, vector, Toonz
  Raster, FX-heavy, camera, audio, and script-heavy scenes.
- Run manual smoke passes on all supported platforms.
- Record known limitations and decide whether they block release.
- Update user-facing docs, developer docs, and troubleshooting docs.
- Decide when Qt 5 CI can be removed or archived.

## Manual Verification Guide

Use this guide for the current Qt 6 port. The goal is to verify real workflows
without confusing automated smoke evidence with user-facing parity.

### Prerequisites

Run from the repository root.

```sh
mise run doctor
mise run doctor-qt6
```

Build and package both lanes when possible:

```sh
mise run build
mise run build-qt6
mise run package-macos-qt6
mise run check-macos-arm64-qt6
```

If the full Qt 5 build is too expensive for a given pass, at least build the
affected target and record the limitation.

Run compatibility guardrails:

```sh
mise run check-no-qregexp
mise run check-core5compat-scope
mise run check-textcodec
mise run check-textcodec-qt6
git diff --check
```

### Automated Preflight Before Manual QA

Run the broad script suite:

```sh
mise run script-smokes-qt6
mise run script-smokes-natural-exit-qt6
```

Run the core packaged GUI smokes:

```sh
mise run gui-smoke-qt6
mise run gui-smoke-scene-create-qt6
mise run gui-smoke-highdpi-qt6
mise run gui-smoke-xsheet-scrub-qt6
mise run gui-smoke-viewer-render-qt6
mise run gui-smoke-viewer-vector-render-qt6
mise run gui-smoke-preview-render-output-qt6
mise run gui-smoke-fx-preview-render-output-qt6
mise run gui-smoke-fx-preview-subcamera-render-output-qt6
mise run gui-smoke-fx-preview-flipbook-controls-qt6
mise run gui-smoke-fx-preview-save-previewed-frames-qt6
mise run gui-smoke-fx-preview-subcamera-save-previewed-frames-qt6
mise run gui-smoke-final-render-output-qt6
mise run gui-smoke-final-render-background-output-qt6
mise run gui-smoke-final-render-sequence-output-qt6
mise run gui-smoke-final-render-composite-output-qt6
mise run gui-smoke-final-render-vector-output-qt6
mise run gui-smoke-final-render-toonz-raster-output-qt6
mise run gui-smoke-final-render-fx-output-qt6
```

Run the multimedia backend preflight:

```sh
mise run gui-smoke-media-devices-qt6
mise run gui-smoke-camera-formats-qt6
mise run gui-smoke-audio-input-qt6
mise run gui-smoke-audio-output-qt6
mise run gui-smoke-audio-recording-wav-qt6
mise run gui-smoke-audio-playback-wav-qt6
```

Run the current tool, overlay, and input smokes relevant to manual parity:

```sh
mise run gui-smoke-viewer-zoom-pan-qt6
mise run gui-smoke-viewer-onion-skin-qt6
mise run gui-smoke-viewer-onion-skin-rowarea-qt6
mise run gui-smoke-viewer-onion-skin-rowarea-drag-qt6
mise run gui-smoke-viewer-onion-skin-fixed-marker-drag-qt6
mise run gui-smoke-viewer-onion-skin-shift-trace-qt6
mise run gui-smoke-viewer-onion-skin-context-menu-qt6
mise run gui-smoke-viewer-onion-skin-custom-colors-qt6
mise run gui-smoke-viewer-onion-skin-orientations-qt6
mise run gui-smoke-viewer-camera-overlay-qt6
mise run gui-smoke-viewer-safe-area-field-guide-qt6
mise run gui-smoke-viewer-safe-area-presets-qt6
mise run gui-smoke-viewer-safe-area-custom-file-qt6
mise run gui-smoke-viewer-field-guide-settings-qt6
mise run gui-smoke-viewer-ruler-guide-qt6
mise run gui-smoke-viewer-ruler-guide-events-qt6
mise run gui-smoke-viewer-ruler-guide-variants-qt6
mise run gui-smoke-viewer-ruler-guide-lines-qt6
mise run gui-smoke-viewer-ruler-guide-styles-qt6
mise run gui-smoke-viewer-ruler-ticks-qt6
mise run gui-smoke-viewer-animate-tool-overlay-qt6
mise run gui-smoke-viewer-animate-tool-drag-qt6
mise run gui-smoke-viewer-animate-tool-mouse-events-qt6
mise run gui-smoke-viewer-animate-tool-undo-redo-qt6
mise run gui-smoke-viewer-animate-tool-modifiers-qt6
mise run gui-smoke-viewer-animate-tool-handles-qt6
mise run gui-smoke-viewer-animate-tool-handle-variants-qt6
mise run gui-smoke-viewer-animate-tool-axis-drags-qt6
mise run gui-smoke-viewer-animate-tool-cursors-qt6
mise run gui-smoke-viewer-selection-tool-vector-handles-qt6
mise run gui-smoke-viewer-selection-tool-vector-handle-variants-qt6
mise run gui-smoke-viewer-selection-tool-vector-center-thickness-deform-qt6
mise run gui-smoke-viewer-selection-tool-vector-mode-variants-qt6
mise run gui-smoke-viewer-selection-tool-raster-handles-qt6
mise run gui-smoke-viewer-selection-tool-raster-mode-variants-qt6
mise run gui-smoke-viewer-vector-brush-qt6
mise run gui-smoke-viewer-raster-brush-qt6
mise run gui-smoke-viewer-raster-brush-mouse-events-qt6
mise run gui-smoke-viewer-raster-brush-tablet-events-qt6
```

`mise run gui-smoke-viewer-raster-brush-system-events-qt6` is a useful
diagnostic when the desktop session allows OS-level automation. Treat failure
from app activation, accessibility trust, or OS event delivery as an environment
blocker until reproduced in an unlocked, frontmost user session.

### Manual Launch With Isolated Runtime Data

Use an isolated runtime root so manual verification does not modify the normal
OpenToonz profile.

```sh
repo_root="$(git rev-parse --show-toplevel)"
manual_root="$repo_root/toonz/build/qt6-manual-verification"
mkdir -p "$manual_root/home" "$manual_root/xdg-config" "$manual_root/xdg-cache"
rm -rf "$manual_root/stuff"
cp -pR "$repo_root/stuff" "$manual_root/stuff"
HOME="$manual_root/home" \
XDG_CONFIG_HOME="$manual_root/xdg-config" \
XDG_CACHE_HOME="$manual_root/xdg-cache" \
"$repo_root/toonz/build/nix-qt6-relwithdebinfo/toonz/OpenToonz.app/Contents/MacOS/OpenToonz" \
  -TOONZROOT "$manual_root/stuff"
```

For a Qt 5 baseline comparison, package or build the Qt 5 lane and launch the
Qt 5 app with a separate isolated runtime root. Do not use the same profile
directory for both lanes during parity checks.

### Manual Verification Checklist

Record the platform, commit, Qt major, build path, display scale, and hardware
available before starting.

#### Startup, Rooms, And Scene Lifecycle

- Launch the Qt 6 app from the packaged bundle.
- Confirm the Startup popup appears and can create a new scene.
- Create a raster scene, save it, close the app, relaunch, and reopen the saved
  scene.
- Create a vector scene and repeat save/reopen.
- Switch rooms and verify panels appear, dock, undock, and redraw.
- Confirm there are no crash dialogs, missing plugin errors, broken icons, or
  repeated OpenGL fallback failures.

#### Viewer And Drawing

- Draw raster strokes with a mouse.
- Draw vector strokes with a mouse.
- Pan, zoom, rotate, reset view, and confirm high-DPI alignment.
- Save and reopen the scene, then confirm strokes redraw correctly.
- Exercise undo/redo across drawing, transform, and selection edits.
- Compare the same operations with the Qt 5 baseline.

Pass criteria:

- No blank viewer.
- No stale framebuffer after edits.
- No large visual mismatch against Qt 5 for the same scene and zoom level.
- No crash or event loss during repeated drawing and undo/redo.

#### Tablet Or Stylus

If hardware is available:

- Test pressure, tilt, hover, eraser, side buttons, device switching, and
  drawing latency.
- Repeat raster and vector drawing.
- Save/reopen and compare output to Qt 5.

If no hardware is available:

- Mark hardware tablet validation as deferred.
- Do not claim tablet parity from the synthetic tablet smoke alone.

#### Selection And Animate/Edit Tools

- Use rectangular, freehand, and polyline selection for vector content.
- Use rectangular, freehand, and polyline selection for raster content.
- Drag scale, rotation, edge scale, center, thickness, and free-deform handles
  where applicable.
- Exercise Animate/Edit Position, Rotation, Scale, Shear, and Center modes.
- Confirm cursor feedback matches the active handle and modifier state.
- Compare with Qt 5 behavior on the same content.

#### Timeline, Xsheet, Onion Skin, Guides, And Overlays

- Scrub the xsheet and timeline.
- Toggle onion skin, fixed markers, relative markers, Shift and Trace, custom
  colors, and orientation-specific controls.
- Use context menus through real mouse input, not only app-side actions.
- Toggle camera box, safe area, field guide, rulers, and guides.
- Create, move, hide, and delete guides manually.
- Verify overlays coexist with drawing, zoom/pan, selection, and room changes.

#### Preview And FX Preview

- Run File > Preview from the UI.
- Run FX Preview from the schematic context menu.
- Verify flipbook frame switching, playback, clone, freeze, and unfreeze.
- Enable sub-camera preview and drag the sub-camera region manually.
- Save previewed frames through the UI.
- Test overwrite and warning dialogs, cancellation, and invalid output paths.
- Run simple raster, vector, Toonz Raster, and production FX-heavy scenes.
- Compare saved outputs and screenshots against Qt 5.

#### Final Render

- Render single frame, frame sequence, composite columns, vector level,
  Toonz Raster level, and FX-heavy scenes.
- Verify dimensions, frame count, alpha/background handling, color, and output
  file naming.
- Reopen rendered images in OpenToonz and in an external viewer.
- Compare against Qt 5 output for representative scenes.

#### Script Console And Script Files

- Open the Script Console.
- Run basic `print`, `warning`, and `run` commands.
- Run scripts that use FilePath, Scene, Level, Image, ImageBuilder,
  Rasterizer, vectorizers, Renderer, and wrapper ids.
- Use `view(Image)` and `view(Level)` from the GUI console.
- Confirm invalid argument errors are visible and do not leave stale state.
- Compare with Qt 5 for external scripts that users actually depend on.

#### Audio, Camera, And Stop-Motion

- Confirm audio input and output devices are listed.
- Record audio through the product UI, save it, insert it, and play it back.
- Verify microphone permission prompts and cancellation behavior.
- Verify audible playback quality and chosen output route.
- Test lip-sync preview timing.
- Confirm camera devices are listed.
- Start live camera preview.
- Capture still frames and insert them into a scene.
- Test stop-motion workflows, including device change and hotplug behavior.

#### Packaging And Runtime Data

- Confirm the packaged Qt 6 app loads Qt frameworks and plugins from the bundle,
  not from the build environment.
- Confirm SVG icons and pixmaps load.
- Confirm multimedia backends load.
- Confirm `stuff` data, rooms, QSS, shaders, brushes, presets, and default
  projects are available.
- Check app behavior after moving the bundle to a clean location.
- For release candidates, repeat with release signing and notarization.

### Manual Verification Record Template

Use this template for each manual pass:

```text
Date:
Commit:
Platform:
Qt lane:
Build command:
Package command:
App bundle:
Display scale:
Input devices:
Audio devices:
Camera devices:

Automated preflight run:
Manual areas covered:
Qt 5 baseline used:
Passes:
Failures:
Deferred because hardware unavailable:
Artifacts:
Release-blocking issues:
Follow-up owner:
```

## Definition Of Done For The Qt 6 Port

The port is done only when all of the following are true:

- Qt 6 builds and packages on every supported release platform.
- Qt 5 remains green until maintainers intentionally retire it.
- Qt Script replacement is product-ready or remaining incompatibilities are
  explicitly accepted.
- Multimedia and stop-motion workflows are verified on real hardware.
- Viewer, drawing, selection, overlays, preview, FX Preview, final render, and
  script-render paths match the Qt 5 baseline closely enough for release.
- Real OS-level input and tablet/stylus behavior are verified or explicitly
  documented as unsupported.
- Release packaging, signing, deployment, runtime data, translations, and docs
  are updated.
- The remaining known gaps are small enough to ship with documented release
  notes.
