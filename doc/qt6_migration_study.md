# OpenToonz Qt 6 Migration Study

Prepared: May 24, 2026

## Executive Recommendation

Porting this checkout from Qt 5 to Qt 6 is feasible, but it is a real migration
project, not a package-version bump. The highest-risk areas are not ordinary
Qt Widgets use. They are the removed Qt Script module, the refactored Qt
Multimedia camera/audio APIs, the OpenGL viewer and offscreen render paths, and
platform packaging behavior.

The recommended strategy is:

1. Keep the current Qt 5 build green while introducing a separate Qt 6 lane.
2. Treat Qt 6 as the active priority. The Metal draft did not reach visual
   parity, so Metal should be revisited after Qt 6 is complete rather than
   blocking this port.
3. Continue Qt 6 work across compile, runtime, script, multimedia, packaging,
   viewer, rendering, drawing, and visual-parity surfaces in incremental
   validated slices.
4. Bring Qt 6 rendering work forward directly from the current OpenToonz
   behavior. Keep changes small and testable, but do not defer viewer,
   renderer, or offscreen GL fixes solely because a Metal draft exists.
5. Treat `Core5Compat` as a temporary bridge only. In the current branch it is
   retained for the legacy text-codec adapter, while `QRegExp` is kept out of
   `toonz/sources` by `mise run check-no-qregexp`. The release-quality port
   should move to maintained Qt 6 APIs wherever practical.

In practical sequencing terms: finish Qt 6 first and use the current OpenToonz
Qt 5 behavior as the visual-parity baseline. Revisit Metal as a separate
follow-up after the Qt 6 port is complete.

## Source Basis

This study combines local repository inspection with current official Qt
documentation:

- Qt Porting Guide: <https://doc.qt.io/qt-6/portingguide.html>
- Qt 5 and Qt 6 CMake compatibility:
  <https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html>
- Qt 6 Clazy porting checks:
  <https://doc.qt.io/qt-6/porting-to-qt6-using-clazy.html>
- Qt 6 removed modules:
  <https://doc.qt.io/qt-6/whatsnew60.html#removed-modules-in-qt-6-0>
- Qt Core changes:
  <https://doc.qt.io/qt-6/qtcore-changes-qt6.html>
- Qt Widgets changes:
  <https://doc.qt.io/qt-6/widgets-changes-qt6.html>
- Qt GUI and OpenGL changes:
  <https://doc.qt.io/qt-6/gui-changes-qt6.html>
- Qt Multimedia changes:
  <https://doc.qt.io/qt-6/qtmultimedia-changes-qt6.html>
- Qt 5 Core Compatibility APIs:
  <https://doc.qt.io/qt-6/qtcore5-index.html>
- `qt_add_executable`:
  <https://doc.qt.io/qt-6/qt-add-executable.html>

## Current Checkout Baseline

This repository is a C++17, CMake-based Qt 5 desktop application. The main
CMake entry point is `toonz/sources/CMakeLists.txt`. The Nix/mise path is the
most reproducible local workflow:

```sh
mise run doctor
mise run configure
mise run build
```

The preferred build directory is `toonz/build/nix-relwithdebinfo`, configured
by `toonz/sources/CMakePresets.json`.

Important local facts:

- `toonz/sources/CMakeLists.txt` requires CMake 3.10, but
  `toonz/sources/CMakePresets.json` already requires CMake 3.21 for preset use.
- The Nix environment in `nix/opentoonz-env.nix` pins Qt 5 through `pkgs.qt5`
  and includes `qtbase`, `qtsvg`, `qtscript`, `qttools`, `qtmultimedia`, and
  `qtserialport`.
- The Nix environment exports Qt 5 paths through `OPENTOONZ_QT_PATH`,
  `OPENTOONZ_QT5_DIR`, and `OPENTOONZ_QT_PLUGIN_DIRS`.
- macOS packaging uses `macdeployqt`, then manually copies selected Qt plugin
  groups and Nix store dylibs.
- Windows CI downloads a custom Qt 5.15.2 build with WinTab support and then
  runs `windeployqt.exe --opengl`.
- Linux CI installs Qt 5 development packages from the distribution, including
  `qtscript5-dev`, `qtmultimedia5-dev`, Qt Wayland, Qt SVG, Qt OpenGL, and Qt
  Serial Port packages.
- `WITH_TRANSLATION=OFF` is the common CI/Nix path, but translation generation
  still uses Qt-specific CMake commands when enabled. The Qt 6 branch now has
  an isolated `WITH_TRANSLATION=ON` lane that builds `OpenToonz`, resolves
  `lrelease` through `Qt6::lrelease`, and verifies the expected 63 generated
  `.qm` files.

## Qt Module Inventory

The top-level CMake currently finds these Qt 5 components:

```cmake
Core
Gui
Network
OpenGL
Svg
Xml
Script
Widgets
PrintSupport
LinguistTools
Multimedia
MultimediaWidgets
SerialPort
UiTools
```

The likely Qt 6 component set is:

```cmake
Core
Gui
Network
OpenGL
OpenGLWidgets
Svg
Xml
Widgets
PrintSupport
LinguistTools
Multimedia
MultimediaWidgets
SerialPort
UiTools
Core5Compat
Qml
```

Notes:

- `OpenGLWidgets` is required for `QOpenGLWidget` in Qt 6.
- `Core5Compat` is a temporary bridge for removed Qt 5 Core APIs. In the
  current branch it is not part of the global Qt Core link set;
  `${QT_CORE5COMPAT_LINK_TARGETS}` is linked only by `image`, `toonzlib`, and
  `OpenToonz` because those targets currently include the legacy text-codec
  adapter. Current source no longer has `QRegExp`/`QRegExpValidator` hits.
- `Qml` is the likely replacement dependency if the Qt Script API is ported to
  `QJSEngine`.
- `Script` has no direct official Qt 6 replacement module. This is a blocking
  product decision, not a simple target rename.

## Local Risk Inventory

The following search results were taken from the pre-migration baseline and are
kept here as the original risk inventory. Later status notes call out items
already addressed in this branch.

| Area | Local evidence | Migration meaning |
|---|---|---|
| Qt CMake bindings | Many `Qt5::` links and `qt5_*` commands across `toonz/sources` CMake files | Need version-aware targets and wrapper commands before a Qt 6 build can configure cleanly |
| Qt Script | 26 source/header files use `QScriptEngine`, `QScriptValue`, `QScriptContext`, or related APIs | Biggest API blocker because Qt Script was removed from Qt 6 |
| Multimedia and capture | 16 files use Qt 5 camera/audio/media classes in app, stop-motion, and sound code | Needs focused API rewrite and real hardware validation |
| Desktop geometry | 19 files use `QDesktopWidget`, `QApplication::desktop`, or `qApp->desktop` | Must move to `QScreen` and widget screen helpers |
| Regular expressions | Baseline had 8 files using `QRegExp` or `QRegExpValidator`; current source is guarded by `mise run check-no-qregexp` | Keep these removed Qt 5 APIs out of `toonz/sources`; use `QRegularExpression` and `QRegularExpressionValidator` for new code |
| Text codecs | 7 files reference `QTextCodec` or text-stream codec APIs | Either bridge with `Core5Compat` or create explicit legacy encoding adapters |
| Qt OpenGL classes | 34 files mention `QOpenGL*`, `QGLWidget`, `QSurfaceFormat`, or Qt offscreen GL classes | Needs Qt 6 module changes and direct viewer/offscreen rendering validation |
| Direct OpenGL calls | More than 100 files in core rendering, tools, FX, and viewer code call GL directly | Qt 6 itself does not remove OpenGL, but platform behavior and packaging change |

## Major Workstreams

### 1. Build System And Dependency Lane

The first technical milestone is to make CMake know about a Qt major version
without changing product behavior.

Recommended changes:

- Add an explicit cache option such as `OPENTOONZ_QT_MAJOR`, defaulting to `5`
  until the Qt 6 lane is ready.
- Add central variables for Qt targets and commands:
  - `Qt${OPENTOONZ_QT_MAJOR}::Core`
  - `Qt${OPENTOONZ_QT_MAJOR}::Widgets`
  - `qt${OPENTOONZ_QT_MAJOR}_wrap_cpp`
  - `qt${OPENTOONZ_QT_MAJOR}_add_resources`
  - `qt${OPENTOONZ_QT_MAJOR}_create_translation`
- Prefer project-local wrapper functions such as `opentoonz_qt_wrap_cpp()` and
  `opentoonz_qt_add_resources()` so each subdirectory does not need custom
  version logic.
- For Qt 6, add `OpenGLWidgets`, `Core5Compat`, and `Qml` only where they are
  truly needed.
- Avoid exporting versionless `Qt::` dependencies from project libraries.
  Versioned targets are clearer for this project because many libraries are
  built together and the Qt major must be explicit.
- Add separate CMake presets:
  - `nix-relwithdebinfo` stays Qt 5.
  - `nix-qt6-relwithdebinfo` configures the Qt 6 lane.
  - `nix-qt6-debug` helps isolate compile failures.
- Add separate mise tasks:
  - `configure-qt6`
  - `build-qt6`
  - `doctor-qt6`
  - later, `package-macos-qt6`

The Nix file should introduce a Qt 6 dependency set next to the existing Qt 5
set rather than replacing it immediately. This allows the project to prove
configuration and partial compilation while preserving the known Qt 5 build.

Current branch status:

- The default lane remains Qt 5. A separate `OPENTOONZ_QT_MAJOR=6` lane now
  exists in CMake, Nix, CMake presets, and mise tasks.
- The Qt target and resource/moc/translation command selection is centralized
  so subdirectories can build against either Qt major without hard-coding
  `Qt5::` targets or `qt5_*` commands.
- The isolated Qt 6 translation lane is build-verified with
  `mise run build-qt6-translations`; this covers `.ts` to `.qm` generation,
  but packaged runtime language switching and localized UI spot checks remain
  manual release verification work.
- The Qt 5 default lane currently compiles and links the `OpenToonz` app target
  with `cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz
  --parallel`.
- The Qt 6 lane currently compiles and links the `OpenToonz` app target with
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel`. This is a compile/link milestone only, not product-ready Qt 6
  support.
- The Qt 6 macOS app bundle now packages and passes the arm64 bundle check with
  `mise run package-macos-qt6` and `mise run check-macos-arm64-qt6`; the
  2026-05-31 post-GUI-relink check inspected 291 packaged arm64 Mach-O files.
- On 2026-05-25, post-relink validation reran the macOS Qt 6 package,
  arm64 bundle check, bounded startup smoke, non-rendering high-DPI diagnostic
  smoke, scene create/open smoke, app-side xsheet scrub smoke, media-device
  enumeration smoke, camera-format smoke, audio-input backend smoke,
  audio-output backend smoke, audio-recording WAV smoke, and audio-playback WAV
  smoke successfully against the current packaged app. The bundle check reports
  the main executable as arm64 and checks 291 packaged Mach-O files for arm64;
  the high-DPI smoke recorded window and screen DPR as 2.00. The
  audio-playback WAV smoke generated a 3,000 ms, 132,300-sample mono WAV at
  44.1 kHz, wrote 264,600 bytes through `AudioWriterWAV`, reloaded it through
  OpenToonz sound I/O, and started then stopped playback through
  `TSoundOutputDevice`.
- The packaged Qt 6 app now passes a bounded Cocoa startup smoke through
  `mise run gui-smoke-qt6`; the latest rerun after packaging still gets through
  startup without a Qt platform/plugin abort. This proves only that the app
  stays alive long enough for startup inspection.
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
- The packaged Qt 6 app now also passes a narrow non-rendering xsheet smoke
  through `mise run gui-smoke-xsheet-scrub-qt6`. The app-side hook creates a
  sandbox scene, creates one raster level and one vector level, inserts raster
  and vector cells into the xsheet, switches current frame and column state,
  saves the scene, and verifies the resulting frame/column/level counts. This
  deliberately avoids drawing, viewer redraw, preview render, and final render
  validation. Those rendering surfaces are now part of the active Qt 6 parity
  work.
- The packaged Qt 6 app now also passes narrow raster and vector viewer
  framebuffer smokes through `mise run gui-smoke-viewer-render-qt6` and
  `mise run gui-smoke-viewer-vector-render-qt6`. The app-side hook creates a
  sandbox scene, inserts either a red raster frame or a red vector stroke,
  captures the `SceneViewer` framebuffer before and after the xsheet update,
  requires changed and red-dominant pixels, and saves the captures beside the
  smoke status file. This proves that simple raster and vector cells can reach
  visible Qt 6 viewer pixels in the packaged app; it does not prove brush
  input, interactive vector drawing, onion skin, overlays, manual FX Preview UI
  and broader FX preview workflows, or Qt 5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now also passes
  `mise run gui-smoke-preview-render-output-qt6`. The app-side hook creates a
  one-frame standard-DPI red raster scene, sets preview render settings to
  320x240 without viewer shrink, renders frame 0 through the `Previewer`
  render-cache path, and verifies a ready 320x240 preview raster with
  `Red pixels: 76800`. This proves the basic Previewer cache/render path for a
  simple raster scene; it does not prove the File > Preview UI, manual
  schematic FX Preview command path, sub-camera preview, flipbook controls,
  manual preview-save popup/overwrite/warning dialogs, production FX-heavy
  preview scenes, or Qt 5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now also passes
  `mise run gui-smoke-fx-preview-render-output-qt6`. The app-side hook creates
  a one-frame standard-DPI red raster scene, starts
  `PreviewFxManager::showNewPreview()` on the column FX, verifies that a
  preview `FlipBook` is attached to the expected FX/xsheet, waits for the
  `renderedFrame` signal, and inspects the `PreviewFxManager` cache raster. It
  verifies a ready 320x240 preview cache image with `Red pixels: 76800`. This
  proves the FX Preview manager/flipbook/cache render path for a simple raster
  scene; it does not prove the manual schematic context-menu command,
  live UI playback/timer behavior, manual save popup/overwrite/warning dialogs,
  production FX-heavy preview scenes, or Qt 5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now also passes
  `mise run gui-smoke-fx-preview-subcamera-render-output-qt6`. The app-side
  hook creates a one-frame standard-DPI red raster scene, enables Preview
  Properties sub-camera preview, sets a 160x120 interest rectangle inside a
  320x240 preview camera, starts `PreviewFxManager::showNewPreview()` on the
  column FX, verifies that the manager reports sub-camera preview active, and
  inspects the `PreviewFxManager` cache raster. It verifies a ready 160x120
  cropped preview cache image with `Red pixels: 6820`. This proves the FX
  Preview sub-camera manager/cache path for a simple raster scene; it does not
  prove the manual schematic context-menu command, live UI sub-camera dragging,
  production FX-heavy preview scenes, or Qt 5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now also passes
  `mise run gui-smoke-fx-preview-flipbook-controls-qt6`. The app-side hook
  creates a two-frame standard-DPI raster scene, starts
  `PreviewFxManager::showNewPreview()` on the column FX, verifies both active
  preview cache frames, switches the preview `FlipBook` to frame 2, clones the
  preview, freezes it, verifies the cloned frozen cache frames, and unfreezes
  it. It verifies frame 0 as 320x240 with `Red pixels: 76800` and frame 1 as
  320x240 with `Green pixels: 76800`. This proves FX Preview flipbook frame
  switching, clone, freeze, and unfreeze behavior for a simple two-frame raster
  scene; it does not prove the manual schematic context-menu command, live UI
  playback/timer behavior, manual save popup/overwrite/warning dialogs,
  production FX-heavy preview scenes, or Qt 5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now also passes
  `mise run gui-smoke-fx-preview-save-previewed-frames-qt6`. The app-side hook
  creates a two-frame standard-DPI raster scene, starts
  `PreviewFxManager::showNewPreview()` on the column FX, waits for both preview
  cache frames, saves the preview `FlipBook` to a PNG sequence through
  `FlipBook::doSaveImages()` with the test-safe completion notification
  suppressed, reloads both exported PNGs through Qt image I/O, and verifies two
  320x240 output frames with `Frame 0 red pixels: 76800` and
  `Frame 1 green pixels: 76800`. This proves FX Preview saved-frame export for
  a simple two-frame raster scene; it does not prove the manual schematic
  context-menu command, live UI playback/timer behavior, manual save
  popup/overwrite/warning dialogs, production FX-heavy preview scenes, or Qt
  5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now also passes
  `mise run gui-smoke-fx-preview-subcamera-save-previewed-frames-qt6`. The
  app-side hook creates a two-frame standard-DPI raster scene, enables Preview
  Properties sub-camera preview, sets a 160x120 interest rectangle inside a
  320x240 preview camera, starts `PreviewFxManager::showNewPreview()` on the
  column FX, waits for both preview cache frames, saves the preview `FlipBook`
  to a PNG sequence through `FlipBook::doSaveImages()` with the test-safe
  completion notification suppressed, reloads both exported PNGs through Qt
  image I/O, and verifies two 160x120 cropped output frames with
  `Frame 0 red pixels: 6820` and `Frame 1 green pixels: 6820`. This proves FX
  Preview sub-camera saved-frame export for a simple two-frame raster scene; it
  does not prove the manual schematic context-menu command, live UI sub-camera
  dragging, manual save popup/overwrite/warning dialogs, production FX-heavy
  preview scenes, or Qt 5-vs-Qt 6 visual parity.
- The packaged Qt 6 app now passes seven focused final-render PNG-output
  smokes:
  `mise run gui-smoke-final-render-output-qt6` and
  `mise run gui-smoke-final-render-background-output-qt6`, plus
  `mise run gui-smoke-final-render-sequence-output-qt6` and
  `mise run gui-smoke-final-render-composite-output-qt6`, plus
  `mise run gui-smoke-final-render-vector-output-qt6`, plus
  `mise run gui-smoke-final-render-toonz-raster-output-qt6`, plus
  `mise run gui-smoke-final-render-fx-output-qt6`. The first app-side hook
  creates a one-frame standard-DPI red raster scene, sends it through
  `MovieRenderer` to a PNG sequence under the isolated GUI smoke root, reloads
  the PNG with Qt image I/O, and requires a 320x240 nonempty output with
  visible red pixels. The second hook renders a partial red raster over an
  opaque blue scene background and requires both red foreground and blue
  background pixels. The third hook renders two xsheet rows through
  `MovieRenderer` and verifies a two-frame 320x240 PNG sequence with a red
  first frame and green second frame. The fourth hook renders two raster levels
  in separate xsheet columns and verifies red and green pixel columns in the
  composited 320x240 PNG output. The vector hook renders a red vector level
  through `MovieRenderer` and verifies red pixels in the 320x240 PNG output.
  The Toonz Raster hook creates a color-mapped `TZP_XSHLEVEL` frame with a red
  palette style, renders it through `MovieRenderer`, and verifies red pixels in
  the 320x240 PNG output. The FX hook wraps a partial red raster scene in a
  `STD_blurFx` root
  generated with `TFxUtil::makeBlur()`, renders that root through
  `MovieRenderer`, and verifies red pixels plus the reported FX root.
  The first prototype exposed a one-pixel render: the source raster frame had
  all 76,800 pixels opaque and red, direct xsheet/column FX bboxes were valid,
  and the unified scene-FX tree without the legacy background wrapper had the
  correct bbox, but the legacy transparent scene background color-card wrapper
  collapsed the rendered result. The transparent-background fix skips that
  no-op wrapper when the scene background alpha is zero. The opaque-background
  probe then exposed a second one-pixel render where infinite color-card bboxes
  were collapsed by finite integer enlargement in the `TRasterFx` public bbox,
  dry-compute, and compute paths. The current renderer fix preserves
  `TConsts::infiniteRectD` through those paths. After rebuild and package
  validation, the transparent smoke passed with `Red pixels: 76800`, and the
  opaque-background smoke passed with `Red pixels: 25200` and
  `Blue pixels: 51600`; the sequence smoke passed with two output frames,
  first-frame `Red pixels: 76800`, and second-frame `Green pixels: 76800`;
  the composite smoke passed with `Red pixels: 36000` and
  `Green pixels: 36000`; the vector smoke passed with `Red pixels: 36316`;
  the Toonz Raster smoke passed with `Red pixels: 76800`;
  the FX smoke passed with `Red pixels: 16396` and
  `FX root: STD_blurFx:0`.
  These probes are still narrower than Render popup UI, movie formats,
  broader production FX-heavy scenes, audio muxing, File > Preview UI/manual
  FX Preview UI, or full Qt 5-vs-Qt 6 render parity.
- On 2026-05-30, the viewer framebuffer capture path was hardened so high-DPI
  sizing is part of the same pass/fail gate as visible pixels. The app-side
  status now records logical viewer size, device-pixel viewer size,
  `SceneViewer::getDevPixRatio()`, Qt widget DPR, screen DPR, a
  logical-to-device probe, and a framebuffer-to-viewer-size probe.
  `viewerRenderProbe=ok` now requires those probes plus changed and
  red-dominant pixels. After rebuild and packaging, the packaged Qt 6
  viewer-render smoke passed with logical viewer size `994x819`, DPR `2` /
  `2.00`, framebuffer `1988x1638`, `771605` changed pixels, and `358756` red
  pixels. The packaged Qt 5 app passed the same smoke with the same
  measurements, giving this guard a current dual-lane baseline.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-zoom-pan-qt6`. The app-side smoke inserts a red
  raster frame, captures the `SceneViewer` framebuffer, applies a `1.35` view
  transform plus a `48,-32` logical-pixel pan scaled through
  `SceneViewer::getDevPixRatio()`, and requires both `viewerRenderProbe=ok` and
  `viewerTransformProbe=ok`. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, scale
  `2.3297 -> 3.1451`, `1303367` changed pixels, and `648404` red pixels. The
  packaged Qt 5 run matched those measurements. This is a direct view-matrix
  zoom/pan and high-DPI framebuffer guard; it does not validate OS-level wheel,
  trackpad, or drag event delivery.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-qt6`. The app-side smoke creates two
  transparent raster levels in one xsheet column, captures the current row with
  onion skin disabled, enables whole-scene mobile onion skin at back offset
  `-1`, and requires `onionSkinProbe=ok`, `viewerRenderProbe=ok`, high-DPI
  framebuffer probes, visible red current-frame pixels, one back onion stage
  player, and an onion-pixel increase. The packaged Qt 6 run reported logical
  viewer size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  `45651` red pixels, and onion pixels `0 -> 42012`. The packaged Qt 5 run
  matched those measurements. This is an app-side onion-skin framebuffer guard;
  it does not validate timeline onion marker UI, interactive onion toggles,
  custom onion colors, overlay placement, or full visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-rowarea-qt6`. This app-side smoke
  creates a three-frame raster scene, captures the xsheet row area, sends Qt
  mouse events to set one relative and one fixed onion marker through
  `RowArea`, double-clicks the current-row onion handle off and back on, then
  requires `onionSkinUiProbe=ok`, `onionSkinProbe=ok`,
  `xsheetRowAreaProbe=ok`, `xsheetRowAreaHighDpiProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI viewer probes, changed row-area pixels,
  visible onion pixels, and visible current-frame red pixels. The packaged Qt
  6 run reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, onion rows `0,2`, MOS count `1`, FOS count `1`, row-area
  changed pixels `516`, row-area non-background pixels `106702`, onion pixels
  `0 -> 42012`, `10455` changed viewer pixels, and `45651` red pixels. The
  packaged Qt 5 run matched those measurements. This covers app-side xsheet
  row-area onion marker creation and current-row enable/disable toggling through
  Qt widget events; it does not validate real OS-level input delivery, custom
  onion colors, broader timeline onion workflows, or full visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-rowarea-drag-qt6`. This app-side smoke
  creates a five-frame raster scene, captures the xsheet row area, sends a Qt
  mouse drag from the row-0 MOS handle to row 4 through `RowArea` and
  `XsheetViewer` drag-tool dispatch, then requires `onionSkinDragProbe=ok`,
  `onionSkinProbe=ok`, `xsheetRowAreaProbe=ok`,
  `xsheetRowAreaHighDpiProbe=ok`, `viewerRenderProbe=ok`, high-DPI viewer
  probes, a four-marker mobile onion range, changed row-area pixels, visible
  onion pixels, and visible current-frame red pixels. The packaged Qt 6 run
  reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, current row `2`, onion rows `0,1,3,4`, MOS count `4`, FOS
  count `0`, MOS range `-2,2`, row-area drag event delivery `true`, row-area
  changed pixels `1076`, row-area non-background pixels `106700`, onion pixels
  `0 -> 71318`, `43180` changed viewer pixels, and `45651` red pixels. The
  packaged Qt 5 run matched those measurements. This covers app-side xsheet
  row-area onion marker drag ranges through Qt widget events; real OS-level
  input delivery, broader timeline onion workflows, and full visual parity
  remain open.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-fixed-marker-drag-qt6`. This app-side
  smoke creates a six-frame raster scene, sets row `3` as the current frame,
  sends Qt drags through the fixed onion-marker row-area handles to add back
  FOS rows `0,1,2`, remove rows `1,2`, and add front FOS rows `5,4`, then
  requires `onionSkinFixedDragProbe=ok`, `onionSkinProbe=ok`,
  `xsheetRowAreaProbe=ok`, `xsheetRowAreaHighDpiProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI viewer probes, changed row-area pixels,
  visible onion pixels, and visible current-frame red pixels. The packaged Qt
  6 run reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, current row `3`, onion rows `0,4,5`, MOS count `0`, FOS count
  `3`, fixed-marker counts add `3`, remove `1`, final `3`, fixed-drag event
  delivery `true/true/true`, row-area changed pixels `768`, row-area
  non-background pixels `105554`, onion pixels `0 -> 34969`, `5952` changed
  viewer pixels, and `45651` red pixels. The packaged Qt 5 run matched those
  measurements. This covers app-side fixed onion-marker add/remove/front range
  drags through Qt widget events; real OS-level input delivery, broader
  timeline onion workflows, and full visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-context-menu-qt6`. This app-side smoke
  creates a five-frame raster scene, builds the onion-skin command menu through
  `OnioniSkinMaskGUI::addOnionSkinCommand`, triggers activate, deactivate,
  extend to scene, limit to level, clear fixed markers, clear relative markers,
  and clear all markers, and requires `onionSkinMenuProbe=ok`,
  `onionSkinProbe=ok`, row-area high-DPI capture, changed row-area pixels,
  visible onion pixels, and visible current-frame red pixels. The packaged Qt
  6 run reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, current row `3`, onion rows `0,1,2`, MOS count `3`, FOS count
  `0`, MOS range `-3,-1`, activate/deactivate/extend/limit/clear fixed/clear
  relative/clear all command status `ok`, row-area changed pixels `820`,
  row-area non-background pixels `106716`, onion pixels `0 -> 91203`, `52610`
  changed viewer pixels, and `45651` red pixels. The packaged Qt 5 run matched
  those measurements. This covers app-side onion-skin context-menu command
  behavior through Qt actions; real OS-level right-click/menu delivery, custom
  onion colors, broader timeline onion workflows, and full visual parity remain
  open.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-custom-colors-qt6`. This app-side smoke
  sets custom back/front onion colors, creates black back/front raster frames so
  the onion tint is visible, keeps a red current frame, enables back and front
  mobile onion markers, and requires `onionSkinCustomColorProbe=ok`,
  `onionSkinViewerColorProbe=ok`, `onionSkinPreferenceColorProbe=ok`,
  `xsheetRowAreaProbe=ok`, `xsheetRowAreaHighDpiProbe=ok`,
  `onionSkinProbe=ok`, `viewerRenderProbe=ok`, high-DPI viewer probes, custom
  hue pixels in the viewer and row area, changed viewer pixels, and visible red
  current-frame pixels. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, custom colors back
  `44,92,224` and front `232,184,28`, viewer custom-color pixels back `54237`
  and front `32761`, row-area custom-color pixels back `3545` and front `915`,
  current row `2`, onion rows `1,3`, MOS count `2`, FOS count `0`, `86998`
  changed viewer pixels, and `57927` red pixels. The packaged Qt 5 run matched
  those measurements. This covers app-side custom onion color preference
  propagation, row-area marker color, and viewer framebuffer tinting; real
  OS-level input/menu delivery, broader timeline onion workflows, and full
  visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-orientations-qt6`. This app-side smoke
  creates a three-frame raster scene, normalizes the xsheet to the
  `TopToBottom` orientation, sends Qt row-area events to create relative and
  fixed onion markers plus current-row off/on toggles, flips to `LeftToRight`,
  and repeats the row-area marker sequence. It requires
  `onionSkinOrientationProbe=ok`, `onionSkinUiProbe=ok`,
  `onionSkinProbe=ok`, `xsheetRowAreaProbe=ok`,
  `xsheetRowAreaHighDpiProbe=ok`, `viewerRenderProbe=ok`, high-DPI viewer
  probes, changed row-area pixels in both orientations, visible onion pixels,
  and visible current-frame red pixels. The packaged Qt 6 run reported logical
  viewer size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  orientation `TopToBottom -> LeftToRight`, vertical row-area changed pixels
  `516`, vertical row-area non-background pixels `106702`, horizontal row-area
  changed pixels `572`, horizontal row-area non-background pixels `14294`,
  onion rows `0,2`, MOS count `1`, FOS count `1`, onion pixels `0 -> 42012`,
  `32761` changed viewer pixels, and `45651` red pixels. The packaged Qt 5 run
  matched those measurements. This covers app-side onion marker creation and
  current-row toggle behavior across both current xsheet/timeline orientations;
  real OS-level input/menu delivery, other timeline onion workflows, and full
  visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-onion-skin-shift-trace-qt6`. This app-side smoke
  creates a five-frame raster scene, enables Shift and Trace, sends Qt
  row-area clicks through the Shift and Trace ghost-marker cells to move the
  back ghost to row `0`, move the front ghost to row `4`, reset through the
  current row, hide both ghosts, and restore both final ghosts. It requires
  `onionSkinShiftTraceProbe=ok`, `onionSkinProbe=ok`,
  `xsheetRowAreaProbe=ok`, `xsheetRowAreaHighDpiProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI viewer probes, changed row-area pixels,
  visible onion pixels, visible current-frame red pixels, and before/after
  captures. The packaged Qt 6 run reported logical viewer size `994x819`, DPR
  `2` / `2.00`, framebuffer `1988x1638`, offsets initial `-1,1`, moved
  `-2,2`, reset `-1,1`, hidden `0,0`, final `-2,2`, event delivery
  `true/true/true/true/true`, current row `2`, stage players `3`, stage onion
  players `2`, back/front onion players `1/1`, onion rows `0,4`, row-area
  changed pixels `2658`, row-area non-background pixels `105636`, onion pixels
  `33805 -> 44099`, `44864` changed viewer pixels, and `47307` red pixels.
  The packaged Qt 5 run matched those measurements. This covers app-side Shift
  and Trace ghost marker move/reset/hide behavior through Qt widget events;
  real OS-level input/menu delivery, additional timeline onion workflows, and
  full visual parity remain open.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-camera-overlay-qt6`. The app-side smoke creates a
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
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-safe-area-field-guide-qt6`. The app-side smoke
  creates a blank sandbox scene, disables the camera box, safe-area, and
  field-guide overlays, captures the `SceneViewer`, enables safe area and field
  guide through `MI_SafeArea` / `safeAreaToggle` and `MI_FieldGuide` /
  `fieldGuideToggle`, and requires `safeAreaProbe=ok`, `fieldGuideProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, changed pixels, red
  safe-area pixels, gray guide pixels, changed gray guide pixels, and
  before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, `112230` changed
  pixels, `5279` red pixels, `645999` gray pixels, and `106951` changed gray
  pixels. The packaged Qt 5 run matched those measurements. This is an
  app-side safe-area and field-guide overlay framebuffer guard only; it does not
  validate selection handles, tool cursor overlays, rulers/guides added through
  ruler UI, OS/menu interaction, safe-area variants beyond the separate
  preset/color and custom-file smokes, field-guide variants beyond the separate
  settings smoke, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-safe-area-presets-qt6`. The app-side smoke creates
  a blank sandbox scene, disables camera, field-guide, ruler, guide, and
  safe-area overlays, captures the `SceneViewer`, enables the default `PR_safe`
  safe-area preset, switches to the bundled custom `150MT_FR_PR_safe` preset,
  and requires `safeAreaPresetProbe=ok`, `viewerRenderProbe=ok`, high-DPI
  framebuffer probes, stored safe-area preset name, visible default red
  safe-area pixels, visible custom red/green/blue safe-area pixels, changed
  green and blue pixels between presets, and saved captures. The packaged Qt 6
  run reported logical viewer size `1006x831`, DPR `2` / `2.00`, framebuffer
  `2012x1662`, preset `PR_safe -> 150MT_FR_PR_safe`, default red pixels `5279`,
  default changed red pixels `5279`, custom red pixels `4884`, custom green
  pixels `2873`, custom blue pixels `3008`, preset-delta changed pixels
  `16043`, preset-delta green pixels `2873`, preset-delta blue pixels `3008`,
  and final red pixels `4884`. The packaged Qt 5 run matched those
  measurements. This covers app-side safe-area preset selection, bundled
  custom-color parsing, and viewer framebuffer rendering; it does not validate
  real title-bar context-menu delivery, persistent user-managed safe-area file
  lifecycle, OS/menu interaction, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-safe-area-custom-file-qt6`. The app-side smoke
  creates a blank sandbox scene, disables camera, field-guide, ruler, guide,
  and safe-area overlays, writes a custom `safearea.ini` into the isolated smoke
  config root, enables the custom `codex_custom_safe_area_probe` preset, and
  requires `safeAreaCustomFileProbe=ok`, `viewerRenderProbe=ok`, high-DPI
  framebuffer probes, custom-file write/restore, stored preset name, visible
  red/green/blue safe-area pixels, changed red/green/blue safe-area pixels, and
  saved captures. The packaged Qt 6 run reported logical viewer size
  `1006x831`, DPR `2` / `2.00`, framebuffer `2012x1662`, custom-file write and
  restore `true`, preset `codex_custom_safe_area_probe`, custom red pixels
  `2724`, custom green pixels `2248`, custom blue pixels `1712`, changed red
  pixels `2724`, changed green pixels `2248`, changed blue pixels `1712`,
  total changed pixels `6684`, and final red pixels `2724`. The packaged Qt 5
  run matched those measurements. This covers app-side user-authored
  `safearea.ini` parsing and viewer framebuffer rendering inside the isolated
  smoke config root; real title-bar context-menu delivery, persistent
  user-profile file management outside the smoke root, OS/menu interaction, and
  full overlay visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-field-guide-settings-qt6`. The app-side smoke
  creates a blank sandbox scene, disables camera, safe-area, field-guide, ruler,
  and guide overlays, captures the `SceneViewer`, applies field-guide size and
  aspect-ratio settings `10` / `1.3333` and then `24` / `2.4000`, and requires
  `fieldGuideSettingsProbe=ok`, `fieldGuideProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, stored field-guide settings, visible gray guide
  pixels in both captures, changed gray guide pixels between the two settings,
  no red pixels, and saved captures. The packaged Qt 6 run reported logical
  viewer size `1006x831`, DPR `2` / `2.00`, framebuffer `2012x1662`,
  field-guide settings `16 -> 10 -> 24`, aspect `1.7778 -> 1.3333 -> 2.4000`,
  first gray pixels `619965`, first changed gray pixels `49276`, second gray
  pixels `699049`, second changed gray pixels `145718`, settings-delta changed
  pixels `141998`, settings-delta gray pixels `119220`, and `0` red pixels.
  The packaged Qt 5 run matched those measurements. This covers app-side
  field-guide size/aspect setting propagation and viewer framebuffer rendering;
  it does not validate field-guide variants outside this size/aspect guard,
  OS/menu interaction, or full overlay visual parity.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-ruler-guide-qt6`. The app-side smoke creates a
  blank sandbox scene, seeds one horizontal and one vertical scene guide,
  disables camera, safe-area, field-guide, guide, and ruler overlays, enables
  the ruler, captures the `SceneViewer` with guides still disabled, enables the
  guide overlay through `MI_ViewGuide` / `viewGuideToggle`, and requires
  `rulerGuideProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
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
  interaction, custom guide positions, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-ruler-guide-events-qt6`. This app-side smoke
  creates a blank sandbox scene, clears guides, enables the ruler and guide
  overlays, resolves the visible horizontal and vertical `Ruler` widgets, then
  sends Qt mouse events to create and move one horizontal and one vertical guide
  and right-click-delete the horizontal guide. It requires
  `rulerGuideEventProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer
  probes, delivered ruler drag events, guide value changes, the horizontal guide
  deletion, the vertical guide remaining, one final scene guide, changed neutral
  guide pixels, two visible ruler widgets, and before/after captures. The
  packaged Qt 6 run reported logical viewer size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, horizontal guides `0 -> 1 -> 0`, vertical guides
  `0 -> 1 -> 1`, horizontal guide value `-77.2788 -> 94.4181 -> deleted`,
  vertical guide value `67.8050 -> -103.8918`, `994` changed pixels, `994`
  changed neutral guide pixels, `539112` gray pixels, `0` changed gray pixels,
  and `0` red pixels. The packaged Qt 5 run matched those measurements. This
  covers app-side ruler-guide create, move, and delete through `Ruler` Qt mouse
  events; it does not validate real OS-level input delivery, ruler tick
  rendering pixels, custom guide positions, ruler drag-hide variants,
  remaining manual ruler-guide
  variants, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-ruler-guide-variants-qt6`. This app-side smoke
  creates a blank sandbox scene, clears guides, enables the ruler and guide
  overlays, resolves the visible horizontal and vertical `Ruler` widgets, then
  creates a horizontal guide and a vertical guide through Qt mouse events,
  drags each one outside its ruler to exercise the drag-hide/delete path, and
  creates final horizontal and vertical guides for a framebuffer delta. It
  requires `rulerGuideVariantProbe=ok`, `viewerRenderProbe=ok`, high-DPI
  framebuffer probes, delivered create/hide/final drag events, guide counts
  returning to zero after each hide, two final scene guides, changed neutral
  guide pixels, two visible ruler widgets, and before/after captures. The
  packaged Qt 6 run reported logical viewer size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, horizontal hide counts `0 -> 1 -> 0 -> 1`,
  vertical hide counts `0 -> 1 -> 0 -> 1`, hide endpoints
  `H 617.00,-8.00` and `V -8.00,529.50`, `1697` changed pixels, `1697`
  changed neutral guide pixels, and `0` red pixels. The packaged Qt 5 run
  matched those measurements. This covers app-side ruler-guide drag-hide
  variants through `Ruler` Qt mouse events; it does not validate real OS-level
  input delivery, every manual ruler-guide variant not covered by app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-ruler-guide-lines-qt6`. This app-side smoke seeds
  two horizontal and two vertical guides at custom world positions, captures the
  `SceneViewer` with guides disabled, enables the guide overlay, moves the
  guides, and captures again. It requires `rulerGuideLineProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, custom guide counts,
  changed neutral guide-line pixels, vertical and horizontal dashed guide-line
  segments, expected guide-position bands, moved-guide framebuffer deltas, and
  two visible ruler widgets. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, guide positions H
  `-96.00,132.00 -> -156.00,48.00` and V
  `-88.00,116.00 -> -136.00,56.00`, first/second guide-line neutral pixels
  `3393` / `3394`, two detected guide columns and two guide rows in each
  capture, dashed segment counts V/H `702/993` then `703/992`, moved
  guide-line neutral pixels `3681`, `6785` changed pixels, and `0` red pixels.
  The packaged Qt 5 run matched those measurements. This covers app-side custom
  guide positions and dashed viewer guide-line rendering; it does not validate
  real OS-level input delivery, every manual ruler-guide variant, guide-line
  visual variants outside this app-side dashed-line guard, or full overlay
  visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-ruler-ticks-qt6`. This app-side smoke creates a
  visible raster scene, disables guide overlays, enables the ruler overlay,
  resolves the visible horizontal and vertical `Ruler` widgets, captures the
  viewer and both ruler widgets before and after a viewer transform change, and
  requires `rulerTickProbe=ok`, `rulerWidgetHighDpiProbe=ok`,
  `rulerTransformProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer and
  widget capture probes, nonzero ruler tick pixels, changed ruler tick pixels,
  changed viewer pixels, visible red content pixels, two visible ruler widgets,
  changed ruler units, and saved widget captures. The packaged Qt 6 run
  reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, horizontal ruler ticks `4098 -> 4074` with `224` changed tick
  pixels, vertical ruler ticks `3410 -> 3374` with `236` changed tick pixels,
  ruler units `124.2500 -> 180.1625` on both rulers, `1425551` changed viewer
  pixels, and `747490` red pixels. The packaged Qt 5 run matched those
  measurements. This covers app-side ruler widget tick rendering after viewer
  transform changes; it does not validate real OS-level input delivery,
  remaining manual ruler-guide variants beyond app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-ruler-guide-styles-qt6`. This app-side smoke
  creates a blank sandbox scene, seeds one horizontal and one vertical guide,
  enables the ruler widgets, captures both `Ruler` widgets, applies a custom
  QSS block through the existing `qproperty-ParentBGColor`,
  `qproperty-ScaleColor`, `qproperty-HandleColor`, and
  `qproperty-HandleDragColor` path, enables the guide overlay, and requires
  `rulerStyleProbe=ok`, `rulerWidgetHighDpiProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer and widget capture probes,
  custom ruler background/handle/scale pixels, changed ruler widget pixels,
  visible ruler widgets, guide counts, and changed neutral guide pixels. The
  packaged Qt 6 run reported logical viewer size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, horizontal ruler style pixels: background
  `0 -> 43570`, handle `0 -> 48`, scale `124`, changed `47712`; vertical
  ruler style pixels: background `0 -> 35858`, handle `0 -> 48`, scale `136`,
  changed `39312`; `1697` changed neutral guide pixels and `0` red pixels. The
  packaged Qt 5 run matched those measurements. This covers the app-side ruler
  QSS/qproperty style path and guide-overlay coexistence; it does not validate
  real OS-level input delivery, remaining manual ruler-guide variants beyond
  app-side create/move/delete/drag-hide, custom viewer guide-line drawing
  styles, or full overlay visual parity.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-animate-tool-overlay-qt6`. The app-side smoke
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
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-animate-tool-drag-qt6`. The app-side smoke creates
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
  Undo/redo integration and modifier-key behavior are covered by separate Qt
  mouse-event smokes below.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-animate-tool-mouse-events-qt6`. The app-side smoke
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
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-animate-tool-undo-redo-qt6`. The app-side smoke
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
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-animate-tool-modifiers-qt6`. The app-side smoke
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
- On 2026-05-30, the packaged Qt 6 app now also passes vector and full-color
  raster brush tool-input smokes through
  `mise run gui-smoke-viewer-vector-brush-qt6` and
  `mise run gui-smoke-viewer-raster-brush-qt6`. The app-side hook creates a
  sandbox level, selects `T_Brush`, replays a direct left-button stroke path
  through the product tool, requires the vector stroke count or raster
  opaque/red pixel counts to increase, captures the `SceneViewer` framebuffer
  before and after the stroke, and requires changed and red-dominant pixels.
  The raster brush hook was also run against the Qt 5 build to verify the smoke
  remains dual-lane compatible. This is still a narrow drawing guard: OS-level
  mouse/tablet event delivery, pressure/tilt, high-DPI input mapping,
  timeline/UI onion workflow, overlays, manual FX Preview UI and broader FX
  preview workflows, broader final-render parity, and visual parity remain
  open.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-raster-brush-mouse-events-qt6`. This smoke sends
  Qt mouse press/move/release events through `SceneViewer`, maps the shared
  world-space brush path into logical widget coordinates using the viewer DPR,
  verifies increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. The same smoke also passed
  against the Qt 5 app build. This narrows Qt widget event dispatch and
  high-DPI input coordinate coverage, but real OS-level CGEvent/System Events
  delivery, tablet input, timeline/UI onion workflow, overlays, manual FX
  Preview UI and broader FX preview workflows, broader final-render parity, and
  visual parity remain open.
- On 2026-05-30, the packaged Qt 6 app now also passes
  `mise run gui-smoke-viewer-raster-brush-tablet-events-qt6`. This smoke sends
  synthetic Qt tablet press/move/release events through `SceneViewer`, maps the
  shared world-space brush path into logical widget coordinates using the
  viewer DPR, drives pressure from 0.35 to 0.85 with tilt `-18,12`, verifies
  increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. The same synthetic tablet
  smoke also passed against the Qt 5 app build. This narrows Qt tablet-event
  dispatch, `TMouseEvent` tablet/pressure propagation, and high-DPI input
  coordinate coverage. Real tablet hardware is not available for this migration
  pass, so hardware pressure/tilt should stay deferred unless a device or
  credible OS-level simulator is added; OS-level CGEvent/System Events
  delivery, platform driver pressure/tilt, timeline/UI onion workflow,
  overlays, manual FX Preview UI and broader FX preview workflows, broader
  final-render parity, and visual parity remain open.
- On 2026-05-30, this branch added the permission-sensitive
  `mise run gui-smoke-viewer-raster-brush-system-events-qt6` gate for real
  macOS OS-level input delivery. The app-side hook creates the same raster brush
  scene, publishes screen coordinates for the shared stroke path, waits for the
  shell-side CGEvent/System Events driver, and requires raster opaque/red pixel
  counts plus `SceneViewer` framebuffer pixels to change. The harness now uses
  a LaunchServices launch path for this permission-sensitive smoke, preserving
  smoke environment and log routing through `open --env`, preflighting
  `CGPreflightPostEventAccess()`, attempting AX raise/frontmost plus bundle/PID
  activation, reporting the frontmost application, reporting target window
  bounds and point containment, posting both the Qt global stroke and a
  diagnostic backing-scaled stroke, and posting a prime click before the drag.
  In the current local session this gate remains blocked: `cgPostEventAccess=true`
  and `axProcessTrusted=true`, the Qt global points are inside the OpenToonz
  window while backing-scaled points are outside it, but the frontmost
  application remains `com.apple.loginwindow`, the app-side event filter records
  `systemMouseAppEventCount=0` and `systemMouseViewerEventCount=0`, raster
  pixels stay at zero, and the System Events click fallback fails with macOS
  error `-25200`. The existing Qt mouse-event raster brush smoke still passes,
  so this is currently a desktop-session activation or OS input delivery gap
  rather than a product brush regression.
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
  rebuilt the Qt 5 app target. A subsequent Scene reload lifecycle slice rebuilt
  the Qt 6 app, refreshed the macOS Qt 6 package, reran the arm64 bundle check,
  invalidated scene-owned Qt 6 `Level` wrappers before `Scene.load()` replaces
  the native scene, added the focused Scene reload edge smoke, reran the
  aggregate Qt 6 script smokes in bounded and natural-exit modes, and rebuilt
  the Qt 5 app target. The current OS-input slice rebuilt the Qt 6 app,
  refreshed the macOS Qt 6 package, reran the arm64 bundle check, added the
  CGEvent/System Events raster brush smoke, recorded the local `-25200` System
  Events blocker, and reran the existing Qt mouse-event raster brush smoke
  successfully. A follow-up harness slice moved this OS-input smoke to
  LaunchServices launch, added CGEvent post-access and target-activation
  diagnostics, confirmed `cgPostEventAccess=true`, and narrowed the remaining
  failure to target activation/event delivery with unchanged raster pixels. A
  2026-05-31 diagnostic slice rebuilt and repackaged Qt 6, reran the arm64
  bundle check, added app-side OS mouse event counters plus shell-side AX,
  frontmost-application, backing-scale, posted-point-set, and target-window
  containment diagnostics, and proved the current failure is below OpenToonz
  input handling: the Qt global points are on the OpenToonz window, but no
  spontaneous mouse events reach the Qt application while `loginwindow` remains
  frontmost; it also rebuilt the Qt 5 app target to verify the shared probe code
  stays dual-lane compatible. The
  viewer high-DPI slice rebuilt both app targets, refreshed the macOS Qt 6 and
  Qt 5 packages, added viewer logical/device/framebuffer probes to the existing
  capture path, reran the Qt 6 arm64 bundle check, and verified that both
  packaged lanes pass the raster viewer-render smoke with matching DPR and
  framebuffer measurements. The follow-up viewer transform slice rebuilt the Qt
  6 app target, refreshed both macOS packages, added the `viewer-zoom-pan`
  smoke, reran the Qt 6 arm64 bundle check, and verified that both packaged
  lanes pass with matching DPR, transform, framebuffer, changed-pixel, and
  red-pixel measurements. The follow-up viewer onion-skin slice rebuilt both
  app targets, refreshed both macOS packages, added `viewer-onion-skin`, reran
  the Qt 6 arm64 bundle check, and verified that both packaged lanes pass with
  matching DPR, framebuffer, stage-player, current-frame red-pixel, and
  onion-pixel measurements. The follow-up row-area onion marker slice rebuilt
  both app targets, refreshed both macOS packages, added
  `viewer-onion-skin-rowarea`, reran the Qt 6 and Qt 5 arm64 bundle checks, and
  verified that both packaged lanes pass with matching DPR, framebuffer,
  row-area high-DPI, row-area changed-pixel, MOS/FOS marker, toggle-state,
  onion-pixel, changed viewer-pixel, and red-pixel measurements. The follow-up
  row-area onion marker drag-range slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-onion-skin-rowarea-drag`, reran the Qt 6
  and Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, row-area high-DPI, row-area changed-pixel, MOS
  range, onion-row, onion-pixel, changed viewer-pixel, and red-pixel
  measurements. The follow-up fixed onion marker drag-range slice rebuilt
  both app targets, refreshed both macOS packages, added
  `viewer-onion-skin-fixed-marker-drag`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, row-area high-DPI, fixed-marker add/remove/front drag delivery,
  FOS add/remove/final counts, onion-row, onion-pixel, changed viewer-pixel,
  and red-pixel measurements. The follow-up onion-skin context-menu command
  slice rebuilt both app targets, refreshed both macOS packages, added
  `viewer-onion-skin-context-menu`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, row-area high-DPI, row-area changed-pixel, onion command,
  MOS/FOS marker, onion-row, onion-pixel, changed viewer-pixel, and red-pixel
  measurements. The follow-up custom onion color slice rebuilt both app
  targets, refreshed both macOS packages, added
  `viewer-onion-skin-custom-colors`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, stored custom color, row-area high-DPI, row-area custom-color
  pixel, viewer custom-color pixel, onion-row, MOS/FOS marker, changed
  viewer-pixel, and red-pixel measurements. The follow-up orientation onion
  marker slice rebuilt both app targets, refreshed both macOS packages, added
  `viewer-onion-skin-orientations`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, orientation flip, row-area high-DPI, vertical and horizontal
  row-area changed-pixel, MOS/FOS marker, onion-row, onion-pixel, changed
  viewer-pixel, and red-pixel measurements. The follow-up Shift and Trace onion
  marker slice
  rebuilt both app targets, refreshed both macOS packages, added
  `viewer-onion-skin-shift-trace`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer,
  row-area high-DPI, Shift and Trace offset transitions, move/reset/hide event
  delivery, stage onion-player counts, onion-row, row-area changed-pixel,
  onion-pixel, changed viewer-pixel, and red-pixel measurements. The follow-up
  camera overlay slice
  rebuilt both app targets, refreshed both macOS packages, added
  `viewer-camera-overlay`, reran the Qt 6 arm64 bundle check, and verified that
  both packaged lanes pass with matching DPR, framebuffer, changed-pixel, and
  red camera-box overlay measurements. The follow-up safe-area/field-guide
  overlay slice rebuilt both app targets, refreshed both macOS packages, added
  `viewer-safe-area-field-guide`, reran the Qt 6 arm64 bundle check, and
  verified that both packaged lanes pass with matching DPR, framebuffer,
  changed-pixel, red safe-area, gray guide, and changed-gray guide
  measurements. The follow-up safe-area preset/color slice rebuilt both app
  targets, refreshed both macOS packages, added `viewer-safe-area-presets`,
  reran the Qt 6 and Qt 5 arm64 bundle checks, and verified that both packaged
  lanes pass with matching DPR, framebuffer, stored safe-area preset name,
  default red safe-area pixels, custom red/green/blue safe-area pixels,
  preset-delta changed/green/blue pixels, and final red-pixel measurements. The
  follow-up safe-area custom-file slice rebuilt both app targets, refreshed both
  macOS packages, added `viewer-safe-area-custom-file`, reran the Qt 6 and Qt 5
  arm64 bundle checks, and verified that both packaged lanes pass with matching
  DPR, framebuffer, custom-file write/restore, stored safe-area preset name,
  custom red/green/blue safe-area pixels, changed red/green/blue pixels, and
  final red-pixel measurements. The follow-up field-guide settings slice rebuilt
  both app targets, refreshed both macOS packages, added
  `viewer-field-guide-settings`,
  reran the Qt 6 and Qt 5 arm64 bundle checks, and verified that both packaged
  lanes pass with matching DPR, framebuffer, stored field-guide size/aspect,
  first/second gray guide pixels, settings-delta changed/gray pixels, and
  red-pixel measurements. The follow-up ruler/guide overlay slice rebuilt both
  app targets, refreshed both macOS packages, added `viewer-ruler-guide`, reran
  the Qt 6 arm64 bundle check, and verified that both packaged lanes pass with
  matching DPR, framebuffer, changed-pixel, changed-neutral-guide-pixel,
  ruler-widget, and guide-count measurements.
  The follow-up ruler-guide event slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-guide-events`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, ruler-widget, guide-count, guide-value,
  create/move/delete, changed-pixel, changed-neutral-guide-pixel, and red-pixel
  measurements.
  The follow-up ruler-guide variant slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-guide-variants`, reran the Qt 6 and
  Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, ruler-widget, horizontal and vertical drag-hide
  guide counts, hide endpoints, changed-neutral-guide-pixel, and red-pixel
  measurements.
  The follow-up ruler tick rendering slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-ticks`, reran the Qt 6 and Qt 5
  arm64 bundle checks, and verified that both packaged lanes pass with matching
  DPR, framebuffer, ruler-widget, ruler tick-pixel, changed tick-pixel, ruler
  unit, changed viewer-pixel, and red-pixel measurements.
  The follow-up ruler/guide style slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-guide-styles`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, ruler-widget high-DPI, custom ruler background,
  handle, and scale color-pixel, changed ruler-pixel,
  changed-neutral-guide-pixel, and red-pixel measurements.
  The follow-up ruler guide-line rendering slice rebuilt both app targets,
  refreshed both macOS packages, added `viewer-ruler-guide-lines`, reran the Qt
  6 and Qt 5 arm64 bundle checks, and verified that both packaged lanes pass
  with matching DPR, framebuffer, custom guide positions, dashed vertical and
  horizontal guide-line segments, expected guide-position bands, moved-guide
  changed-neutral-pixel, changed-pixel, and red-pixel measurements.
  The follow-up Animate tool overlay slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-animate-tool-overlay`, reran the Qt 6 and
  Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, `T_Edit`/`Col1` state, changed-pixel, and red-pixel
  measurements.
  The follow-up Animate tool drag slice rebuilt both app targets, refreshed both
  macOS packages, added `viewer-animate-tool-drag`, reran the Qt 6 and Qt 5
  arm64 bundle checks, and verified that both packaged lanes pass with matching
  DPR, framebuffer, `T_Edit`/`Col1` state, `TStageObject` X/Y movement,
  changed-pixel, and red-pixel measurements.
  The follow-up Animate tool mouse-event slice rebuilt both app targets,
  refreshed both macOS packages, added `viewer-animate-tool-mouse-events`, reran
  the Qt 6 and Qt 5 arm64 bundle checks, and verified that both packaged lanes
  pass with matching DPR, framebuffer, Qt mouse-event dispatch, `T_Edit`/`Col1`
  state, `TStageObject` X/Y movement, changed-pixel, and red-pixel
  measurements.
  The follow-up Animate tool undo-redo slice rebuilt the Qt 6 and Qt 5 app
  targets as needed, refreshed both macOS packages, added
  `viewer-animate-tool-undo-redo`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer, Qt
  mouse-event dispatch, `T_Edit`/`Col1` state, `TStageObject` X/Y movement,
  undo/redo restoration, undo history indices, changed-pixel, and red-pixel
  measurements.
  The follow-up Animate tool modifier slice rebuilt the Qt 6 and Qt 5 app
  targets as needed, refreshed both macOS packages, added
  `viewer-animate-tool-modifiers`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer, Qt
  mouse-event dispatch, `T_Edit`/`Col1` state, normal/Alt/Shift `TStageObject`
  deltas, Alt precision ratio, Shift dominant-axis lock, precise-cursor state,
  changed-pixel, and red-pixel measurements.
  The follow-up Animate tool active-axis drag slice rebuilt the Qt 6 and Qt 5
  app targets as needed, refreshed both macOS packages, added
  `viewer-animate-tool-axis-drags`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer, Qt
  mouse-event dispatch, `T_Edit`/`Col1` state, Position X/Y movement, Rotation
  angle movement, Scale movement, Shear X/Y movement, Center movement,
  changed-pixel, and red-pixel measurements.
  The follow-up Animate tool cursor slice rebuilt the Qt 6 and Qt 5 app targets
  as needed, refreshed both macOS packages, added
  `viewer-animate-tool-cursors`, reran the Qt 6 and Qt 5 arm64 bundle checks,
  and verified that both packaged lanes pass with matching DPR, framebuffer, Qt
  mouse-event dispatch, `T_Edit`/`Col1` state,
  Position/Rotation/Scale/Shear/Center cursor IDs, Alt precise cursor
  decoration, cursor pixmap availability, normal/Alt cursor artwork signatures,
  changed-pixel, and red-pixel measurements.
  The follow-up Selection tool vector handle slice rebuilt both app targets as
  needed, refreshed both macOS packages, added
  `viewer-selection-tool-vector-handles`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, rectangular
  selection mode/type, selected stroke count, selected bbox scale-handle drag
  deltas, scale cursor ID/artwork availability, changed-pixel, and red-pixel
  measurements.
  The follow-up Selection tool vector handle-variant slice rebuilt both app
  targets as needed, refreshed both macOS packages, added
  `viewer-selection-tool-vector-handle-variants`, reran the Qt 6 and Qt 5 arm64
  bundle checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, rectangular
  selection mode/type, selected stroke count, horizontal edge-scale delta,
  vertical edge-scale delta, rotation bbox delta, handle cursor IDs/artwork
  availability, changed-pixel, and red-pixel measurements.
  The follow-up Selection tool vector center/thickness/free-deform slice rebuilt
  both app targets as needed, refreshed both macOS packages, added
  `viewer-selection-tool-vector-center-thickness-deform`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, Qt mouse-event dispatch, `T_Selection` state,
  rectangular selection mode/type, selected stroke count, center-handle delta,
  average-thickness delta, Ctrl free-deform bbox delta, handle cursor
  IDs/artwork availability, changed-pixel, and red-pixel measurements.
  The follow-up Selection tool vector mode slice rebuilt both app targets as
  needed, refreshed both macOS packages, added
  `viewer-selection-tool-vector-mode-variants`, reran the Qt 6 and Qt 5 arm64
  bundle checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, Freehand and
  Polyline vector selection types, two separated vector strokes, Freehand
  selecting only stroke `0`, Polyline selecting only stroke `1`, distinct
  selected bboxes, changed-pixel, and red-pixel measurements.
  The follow-up Selection tool raster handle slice rebuilt both app targets as
  needed, refreshed both macOS packages, added
  `viewer-selection-tool-raster-handles`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, rectangular raster
  selection type, non-empty raster selection, floating-selection state after
  scale-handle drag, changed raster selection bbox, scale cursor ID/artwork
  availability, changed-pixel, and red-pixel measurements.
  The follow-up Selection tool raster mode slice rebuilt both app targets as
  needed, refreshed both macOS packages, added
  `viewer-selection-tool-raster-mode-variants`, reran the Qt 6 and Qt 5 arm64
  bundle checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, Freehand and
  Polyline raster selection types, non-empty non-floating raster selections,
  distinct selected bboxes, changed-pixel, and red-pixel measurements.
  The follow-up Animate tool handle-variant slice rebuilt both app targets as
  needed, refreshed both macOS packages, added
  `viewer-animate-tool-handle-variants`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Edit`/`Col1` state, All-mode
  fallback translation movement, ScaleXY X/Y movement, Shear X/Y movement,
  changed-pixel, and red-pixel measurements.
- The packaged Qt 6 app now also passes a non-rendering high-DPI diagnostic
  smoke through `mise run gui-smoke-highdpi-qt6`. The app-side hook records the
  main-window DPR, screen DPR, logical DPI, screen geometry, and the expected
  Qt 6 always-on high-DPI mode. This is a startup/window-state guard only; it
  does not validate viewer scaling, canvas pixels, tool overlays, or render
  output.
- First removed-API fixes in this branch include `QDesktopWidget` to `QScreen`
  helper usage, `QMutex::Recursive` to `QRecursiveMutex`, Qt 6 audio playback
  construction, `QMatrix` to `QTransform` in `iwa_floorbumpfx.cpp`, and local
  `qsizetype` size fixes in `iwa_particlesengine.cpp`.
- Direct `QTextCodec` usage is now isolated behind `ttextcodec.h`. The current
  adapter preserves the legacy Shift-JIS, GBK, UTF-8, and PSD layer-name paths
  while keeping the remaining Core5Compat dependency auditable.
- The legacy text-codec adapter now has explicit Qt 5 and Qt 6 regression
  checks through `mise run check-textcodec` and
  `mise run check-textcodec-qt6`. They compile a small adapter consumer in the
  matching Nix shell and verify exact Shift-JIS and GBK byte sequences plus the
  unknown-codec UTF-8 fallback before any future Core5Compat removal attempt.
- `mise run check-no-qregexp` now fails if `QRegExp` or `QRegExpValidator`
  reappears under `toonz/sources`, keeping that removed Qt 5 API out of the
  active Qt 6 migration surface.
- `mise run check-core5compat-scope` now fails if direct `QTextCodec` usage
  escapes `ttextcodec.h`, if a new adapter consumer is added without updating
  the documented scope, or if Core5Compat references appear outside the
  adapter-owning CMake files.
- `mise run check-no-qregexp` and `mise run check-core5compat-scope` now also
  run before the normal local configure, build, Qt 6 configure, and Qt 6
  translation-build tasks, so removed regex APIs and adapter-scope regressions
  fail before CMake/Nix work starts.
- Core5Compat is now scoped out of `${QT_CORE_LINK_TARGETS}` and into the
  dedicated `${QT_CORE5COMPAT_LINK_TARGETS}` variable. The only current target
  links are `image`, `toonzlib`, and `OpenToonz`, matching the adapter-owning
  code paths. After this link-scope reduction, both Qt 6 and Qt 5 `OpenToonz`
  app targets still build.
- Direct `QGLWidget::convertToGLFormat` usage has been removed from
  `tnztools/toolutils.cpp` and `tnztools/skeletontool.cpp`. Both call sites now
  use `QtCompat::convertToGLFormat`, which keeps the OpenGL texture-image
  conversion Qt-version-aware without retaining a direct `QGLWidget` dependency.
- The SXF direct-text popup now uses `QCheckBox::checkStateChanged` on Qt 6.7+
  and the existing `stateChanged` signal on Qt 5, removing a Qt 6 deprecation
  warning without changing the Qt 5 lane.
- Stop-motion camera-option sizing and legacy Pencil Test camera-label sizing
  now use `QtCompat::fontMetricsHorizontalAdvance()`, removing direct
  `QFontMetrics::width()` calls from that warning slice while preserving the
  Qt 5 fallback.
- The Qt 6 startup path no longer sets the removed/deprecated high-DPI
  application attributes; Qt 6 uses its always-on high-DPI behavior, while the
  Qt 5 lane keeps the existing attributes. The translator startup path also
  uses `QLibraryInfo::path()` on Qt 6 and explicitly discards optional
  `QTranslator::load()` results. After this cleanup, the current app-target
  warning frontier in `main.cpp` is limited to OpenGL/GLUT deprecation output.
- `mise run check-qt6-highdpi-attribute-scope` now guards that
  `AA_EnableHighDpiScaling` and `AA_UseHighDpiPixmaps` stay inside
  `QT_VERSION < QT_VERSION_CHECK(6, 0, 0)` startup blocks.
- Remaining user-activated `QComboBox::activated(int)` connects in Pencil
  Test, Camera Track export, and Startup preset selection now go through
  `QtCompat::connectComboBoxActivatedIndex()`, which uses `textActivated` and
  forwards the combo box current index. `mise run
  check-qt6-combobox-activated-scope` keeps direct `QComboBox::activated`
  connects out of the current source tree.
- Remaining direct checkbox `stateChanged(int)` connects in the stop-motion
  controller and Type tool options now also route through
  `QtCompat::connectCheckStateChanged()`. `mise run
  check-qt6-checkbox-state-scope` keeps direct `QCheckBox::stateChanged`
  connects out of feature code so Qt 6.7+ can use `checkStateChanged` while the
  Qt 5 lane keeps the existing signal path.
- Legacy `toonzfarm/tfarm/tbaseserver.cpp` Windows socket diagnostic formatting
  no longer uses unbounded `wsprintf()` calls. The file now uses bounded
  `snprintf()` formatting for those messages and initializes the non-Windows
  send failure message before throwing.
- Later compile-frontier fixes in this branch include removal of stale
  `QDirModel` usage, removal of remaining `QRegExp` hits, Qt 6-safe layout
  margin calls, Qt 6-safe MOC guards using `OPENTOONZ_QT_MAJOR`, a stop-motion
  camera-info helper around `QMediaDevices`/`QCameraDevice`, and removal of a
  stale `FileInfoPopup` MOC entry that generated an invalid Qt 6 metatype
  reference.
- Qt 6 builds still emit macOS OpenGL/GLUT deprecation warnings. Those are not
  treated as compile failures, but the related GL/viewer behavior is now part
  of the active Qt 6 visual-parity work.

### 2. Qt 5.15-Compatible API Cleanup

The official Qt guidance is to make the Qt 5 application clean on Qt 5.15
before migrating. This checkout already builds against Qt 5.15.x in the Nix
path, so the next step is to make deprecated APIs fail in a controlled lane.

Recommended setup:

```cmake
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x050F00)
```

Do not enable that globally at first. Add it behind a focused option or CI task,
because it will likely produce many failures. The initial value of the task is
diagnostic: it should list work, not break the default build.

High-value cleanup that can be done before Qt 6:

- Replace `QDesktopWidget` and `qApp->desktop()` with helpers based on
  `QGuiApplication::primaryScreen()`, `QGuiApplication::screens()`, and
  `QWidget::screen()`.
- Keep `QRegExp` and `QRegExpValidator` out of `toonz/sources` with
  `mise run check-no-qregexp`; use `QRegularExpression` and
  `QRegularExpressionValidator` for any future regex code.
- Keep `QGLWidget::convertToGLFormat` out of feature code. The former direct
  uses in `tnztools/toolutils.cpp` and `tnztools/skeletontool.cpp` now route
  through `QtCompat::convertToGLFormat`.
- Keep legacy text encoding behavior behind `ttextcodec.h`. The currently
  covered cases are Shift-JIS, GBK, UTF-8, PSD layer names, simple-level PSD
  names, and SXF import/export; future text-codec work should extend that
  adapter rather than reintroducing direct `QTextCodec` usage. Core5Compat is
  currently linked only into `image`, `toonzlib`, and `OpenToonz` for this
  adapter; use `mise run check-textcodec` and
  `mise run check-textcodec-qt6` as the behavior guard and
  `mise run check-core5compat-scope` as the link/include scope guard before
  changing it, then remove those target-specific links when the adapter no
  longer needs the Qt 6 compatibility module.
- Keep direct deprecated `QFontMetrics::width()` calls behind
  `QtCompat::fontMetricsHorizontalAdvance()`. `mise run
  check-qt6-fontmetrics-scope` catches feature-code regressions while allowing
  the Qt 5 fallback inside the compatibility helper.
- Run Clazy's Qt 6 checks against a Qt 5 build and apply fixits only in small,
  reviewable slices.

### 3. Qt Script Replacement

This is the most important non-rendering blocker.

Qt 6.0 removed `Qt Script` and `Qt Script Tools`. The current checkout links
`Qt5::Script` in `toonzlib` and the main `OpenToonz` target. The script system
uses:

- `QScriptEngine`
- `QScriptEngineDebugger`
- `QScriptContext`
- `QScriptProgram`
- `QScriptValue`
- `QScriptable`
- `qscriptvalue_cast`
- `QScriptEngine::newFunction`
- `QScriptEngine::newQObject`
- many `Q_INVOKABLE QScriptValue ...` script binding APIs

The recommended direction is to port to `QJSEngine` from Qt Qml, but not by
doing a blind type rename. The APIs are shaped differently.

Migration design:

1. Add a project-owned scripting facade, for example `ScriptRuntime`, that
   exposes the operations OpenToonz needs: evaluate code, run a script file,
   interrupt or cancel if possible, publish global functions, bind scene/image
   wrapper objects, call back onto the main thread, report output, and report
   source-line errors.
2. Move current `QScriptEngine` usage behind that facade while still building
   on Qt 5.
3. Add a `QJSEngine` implementation of the facade.
4. Rebuild the script bindings around QObject wrappers, `Q_INVOKABLE` methods,
   `QJSValue`, and small argument-parsing helpers.
5. Preserve the external script API where possible by injecting JavaScript
   compatibility wrappers for global functions like `print`, `warning`, and
   `run`.
6. Decide what to do with the debugger path. `QScriptEngineDebugger` has no
   direct drop-in equivalent in the target Qt 6 API set.

Current branch status:

- Qt 5 still builds the existing `QScriptEngine` runtime and binding classes.
- Qt 6 builds a first `QJSEngine` runtime shell for `ScriptEngine`, including
  JavaScript evaluation plus `print`, `warning`, and `run` bootstrap functions.
- The Qt 6 bootstrap no longer assumes `globalThis`, which keeps the
  compatibility shell usable in older JavaScript global-object contexts.
- `ScriptEngine::wait()` now performs idempotent executor cleanup after
  command-line and other non-event-loop evaluations finish.
- The Qt 6 `ScriptEngine` QObject wrapper is explicitly marked with
  `QJSEngine::CppOwnership`. Without that, `QJSEngine` tried to own the
  stack-local script engine during teardown and command-line scripts could
  crash instead of exiting naturally.
- `toonz/sources/tests/scriptengine/basic.toonzscript` is the first repo-local
  Qt 6 script fixture. `mise run script-smoke-qt6` runs it through the Qt 6 app
  bundle in headless `QCoreApplication` script mode and verifies `print`,
  `warning`, `run`, the legacy `ToonzVersion` global, and the legacy global
  `void` object returned by `print`/`warning` to suppress unwanted top-level
  evaluation-result output. The `run()` check executes
  `toonz/sources/tests/scriptengine/run_child.toonzscript`, which the smoke
  harness copies into the isolated `stuff/library/scripts` tree to cover the
  same library-script lookup path as the legacy Qt 5 runtime.
- `toonz/sources/tests/scriptengine/run_errors.toonzscript` extends the
  bootstrap fixture set. It is run by `mise run script-smoke-run-errors-qt6`
  and verifies that Qt 6 `run()` throws script-visible errors for missing
  arguments, extra arguments, non-file-path arguments, and missing script files,
  matching the legacy Qt Script failure contract instead of silently returning
  `undefined`.
- `toonz/sources/tests/scriptengine/file_path.toonzscript` is the first
  repo-local Qt 6 object-binding compatibility fixture. It is run by
  `mise run script-smoke-filepath-qt6` and verifies a narrow JavaScript
  `FilePath` facade backed by `TFilePath` helpers.
- `toonz/sources/tests/scriptengine/file_path_coercion.toonzscript` extends
  the FilePath fixture set. It is run by
  `mise run script-smoke-filepath-coercion-qt6` and verifies
  `FilePath.valueOf()`, `String(filePath)`, JavaScript string concatenation,
  copy construction from another `FilePath`, and chained path mutation helpers.
- `toonz/sources/tests/scriptengine/file_path_edges.toonzscript` extends the
  FilePath fixture set. It is run by
  `mise run script-smoke-filepath-edges-qt6` and verifies relative concat,
  absolute-path concat rejection, non-directory `files()` errors, and directory
  listing through the Qt 6 facade.
- `toonz/sources/tests/scriptengine/file_path_metadata.toonzscript` extends the
  FilePath fixture set. It is run by
  `mise run script-smoke-filepath-metadata-qt6` and verifies `exists`,
  `isDirectory`, directory listing, and legacy-style `lastModified` exposure as
  a JS `Date` object for existing files, directories, and missing paths.
- `toonz/sources/tests/scriptengine/file_path_mutation_metadata.toonzscript`
  extends the FilePath fixture set. It is run by
  `mise run script-smoke-filepath-mutation-metadata-qt6` and verifies mutable
  `extension`, `name`, and `parentDirectory` properties,
  `withParentDirectory()` string/FilePath conversion, `exists`, `isDirectory`,
  and `lastModified` exposure as a JS `Date`.
- `toonz/sources/tests/scriptengine/path_arguments.toonzscript` extends
  script path-argument validation. It is run by
  `mise run script-smoke-path-arguments-qt6` and verifies legacy-style
  string/FilePath argument checking for `run()`, FilePath parent/concat
  helpers, Image/Level/Scene path methods, and constructor edge behavior that
  should not silently coerce arbitrary values through `String()`.
- `toonz/sources/tests/scriptengine/scene_basic.toonzscript` is the first
  repo-local Qt 6 scene-binding compatibility fixture. It is run by
  `mise run script-smoke-scene-qt6` and verifies a narrow JavaScript `Scene`
  facade backed by real `ToonzScene` instances.
- `toonz/sources/tests/scriptengine/level_basic.toonzscript` is the first
  repo-local Qt 6 level-binding compatibility fixture. It is run by
  `mise run script-smoke-level-qt6` and verifies empty level handles, vector
  level creation through `Scene.newLevel()`, lookup/listing through `Scene`,
  name mutation, frame-count/frame-id access, and string/type reporting.
- `toonz/sources/tests/scriptengine/image_basic.toonzscript` is the first
  repo-local Qt 6 image-binding compatibility fixture. It is run by
  `mise run script-smoke-image-qt6` and verifies loading an existing PNG,
  inspecting image type/size/DPI/string data, saving a raster image into the
  isolated script-smoke build directory, inserting that image into a raster
  `Level`, and reading the frame back by frame id and frame index.
- `toonz/sources/tests/scriptengine/scene_cells.toonzscript` is the first
  repo-local Qt 6 scene-cell compatibility fixture. It is run by
  `mise run script-smoke-scene-cells-qt6` and verifies setting cells by `Level`
  object, copying cells through the object returned by `Scene.getCell()`,
  setting cells by level name, clearing cells, and preserving xsheet
  frame/column counts.
- `toonz/sources/tests/scriptengine/scene_cell_fids.toonzscript` is the
  repo-local Qt 6 scene-cell frame-id type fixture. It is run by
  `mise run script-smoke-scene-cell-fids-qt6` and verifies legacy
  `Scene.getCell().fid` parity: numeric frame ids return as numbers, and
  lettered frame ids return as strings.
- `toonz/sources/tests/scriptengine/scene_edges.toonzscript` is the
  repo-local Qt 6 advanced Scene edge-case fixture. It is run by
  `mise run script-smoke-scene-edges-qt6` and verifies non-rendering scene API
  parity around Raster/ToonzRaster/Vector level creation, missing level lookup,
  duplicate level errors, bad level-type errors, invalid cell object errors,
  missing-level cell assignment errors, rejection of non-string, non-`Level`
  cell level arguments, four-argument undefined level rejection, bad row/column
  errors, and load-level missing-file/duplicate-name errors.
- `toonz/sources/tests/scriptengine/scene_argument_edges.toonzscript` is the
  repo-local Qt 6 Scene argument-validation fixture. It is run by
  `mise run script-smoke-scene-argument-edges-qt6` and verifies non-rendering
  row/column argument rejection for `insertColumn()`, `deleteColumn()`,
  `getCell()`, and `setCell()`, plus backend negative row/column and bad
  frame-id errors without entering scene icon, viewer, offscreen GL, or
  renderer paths.
- `toonz/sources/tests/scriptengine/scene_lifecycle_edges.toonzscript` is the
  repo-local Qt 6 Scene lifecycle fixture. It is run by
  `mise run script-smoke-scene-lifecycle-edges-qt6` and verifies cross-scene
  level isolation, scene disposal errors, and invalidated scene-owned level
  wrappers without entering viewer, offscreen GL, or renderer paths.
- `toonz/sources/tests/scriptengine/scene_level_wrappers.toonzscript` is the
  repo-local Qt 6 Scene level-wrapper fixture. It is run by
  `mise run script-smoke-scene-level-wrappers-qt6` and verifies
  `Scene.getLevels()` wrapper identity and lifetime behavior, including
  disposal of returned wrappers without deleting the scene-owned level, fresh
  wrapper lookup after disposal, and the current empty-wrapper behavior for
  stale scene-owned level wrappers after `Scene.dispose()`.
- `toonz/sources/tests/scriptengine/level_io.toonzscript` is the first
  repo-local Qt 6 level-I/O compatibility fixture. It is run by
  `mise run script-smoke-level-io-qt6` and verifies standalone empty `Level()`
  frame creation through `Level.setFrame()`, `Level.save()` to a real raster
  level path, and `Level.load()`/`new Level(path)` loading that saved level back
  through scene-decoded OpenToonz level I/O.
- `toonz/sources/tests/scriptengine/level_io_types.toonzscript` is the
  repo-local Qt 6 level-I/O type coverage fixture. It is run by
  `mise run script-smoke-level-io-types-qt6` and verifies standalone
  `Level.save()`/`Level.load()` coverage for a Vector level with frame-image
  access and a ToonzRaster level with type/frame metadata plus
  `Level.getFrame()` and `Level.getFrameByIndex()` access after reloading the
  saved ToonzRaster level.
- `toonz/sources/tests/scriptengine/scene_load_level.toonzscript` is the first
  repo-local Qt 6 scene load-level compatibility fixture. It is run by
  `mise run script-smoke-scene-loadlevel-qt6` and verifies `Scene.loadLevel()`
  by loading a saved raster level into a `Scene`, checking the loaded level
  metadata, and assigning the loaded level to an xsheet cell.
- `toonz/sources/tests/scriptengine/scene_load_level_sequence.toonzscript`
  extends the scene load-level coverage. It is run by
  `mise run script-smoke-scene-loadlevel-sequence-qt6` and verifies multi-frame
  `Scene.loadLevel()` behavior, `Scene.getLevels()` / `Scene.getLevel()`
  registry lookup, name-based and object-based `Scene.setCell()` assignment,
  and a headless `Scene.save()` / `new Scene(path)` roundtrip for the loaded
  sequence.
- `toonz/sources/tests/scriptengine/scene_save_reopen.toonzscript` is the
  repo-local Qt 6 scene data save/reopen compatibility fixture. It is run by
  `mise run script-smoke-scene-save-reopen-qt6` and verifies a headless
  `Scene.save()`/`new Scene(path)` data roundtrip for a scene that references a
  saved raster level. The Qt 6 script save path skips scene-icon generation in
  headless script mode, so this fixture deliberately does not claim scene icon,
  viewer, offscreen GL, or renderer parity.
- `toonz/sources/tests/scriptengine/scene_reload_edges.toonzscript` is the
  repo-local Qt 6 Scene reload lifecycle fixture. It is run by
  `mise run script-smoke-scene-reload-edges-qt6` and verifies loading a saved
  scene into an existing `Scene` facade, replacing it with a second saved scene,
  restoring the first scene, checking the loaded level/cell state after each
  `Scene.load()`, and proving stale scene-owned `Level` wrappers are invalidated
  rather than kept live.
- `toonz/sources/tests/scriptengine/scene_load_failure.toonzscript` is the
  repo-local Qt 6 Scene failed-load lifecycle fixture. It is run by
  `mise run script-smoke-scene-load-failure-qt6` and verifies that
  `Scene.load()` rejects an existing non-scene file before mutating the native
  scene, preserves current scene/cell data, and keeps existing scene-owned
  `Level` wrappers valid until a replacement scene successfully loads.
- `toonz/sources/tests/scriptengine/scene_save_icon.toonzscript` is the
  repo-local Qt 6 scene-icon save compatibility fixture. It is run by
  `mise run script-smoke-scene-save-icon-qt6` and verifies the
  `QApplication` script path for `Scene.save()` scene-icon generation by saving
  a scene, checking the generated `sceneIcons` PNG, and loading that icon back
  through the Qt 6 `Image` facade. This is a narrow offscreen-rendering
  boundary check; under Qt's `offscreen` platform the Qt 6 scene-icon save path
  writes a valid background-filled icon instead of entering `TOfflineGL`, which
  avoids the unsupported offscreen OpenGL context path used only by automation.
  Normal GUI/platform sessions still use the existing rendered scene-icon path.
  This does not claim scene-icon visual parity, viewer parity, or broader
  render-output parity.
- `toonz/sources/tests/scriptengine/scene_save_icon_variants.toonzscript` is
  the repo-local Qt 6 scene-icon variant compatibility fixture. It is run by
  `mise run script-smoke-scene-save-icon-variants-qt6` and verifies the
  `QApplication` scene-icon path for a two-column raster scene plus a second
  save, checking that both generated `sceneIcons` PNGs load through the Qt 6
  `Image` facade with stable nonzero dimensions. This remains a
  scene-icon/offscreen boundary check; it does not claim pixel-level icon
  parity, rendered icon parity, or viewer parity.
- `toonz/sources/tests/scriptengine/scene_columns.toonzscript` is the
  repo-local Qt 6 Scene/xsheet column mutation fixture. It is run by
  `mise run script-smoke-scene-columns-qt6` and verifies populated cells shift
  across inserted columns, deleted columns remove their cells, remaining
  columns shift left, and frame/column counts remain consistent without
  entering viewer, offscreen GL, or renderer paths.
- `toonz/sources/tests/scriptengine/scene_frameids.toonzscript` is the
  repo-local Qt 6 Scene/Level frame-id compatibility fixture. It is run by
  `mise run script-smoke-scene-frameids-qt6` and verifies lettered `TFrameId`
  handling across `Level.setFrame()`, `Level.getFrame()`,
  `Level.getFrameByIndex()`, `Scene.setCell()`, `Scene.getCell()`, and a
  headless `Scene.save()`/`new Scene(path)` roundtrip. It deliberately stays on
  scene data I/O and does not claim scene icon, viewer, offscreen GL, or
  renderer parity.
- `toonz/sources/tests/scriptengine/level_edges.toonzscript` is the repo-local
  Qt 6 Level edge-case compatibility fixture. It is run by
  `mise run script-smoke-level-edges-qt6` and verifies empty-level frame access
  errors, empty-level save errors, missing-frame `undefined` behavior,
  out-of-range and non-number frame-index errors, bad frame-id rejection,
  empty-level name setter no-op parity, empty-level path `undefined` parity,
  level/image type mismatch rejection, incompatible save rejection, and
  missing-level load errors.
- `toonz/sources/tests/scriptengine/image_edges.toonzscript` is the repo-local
  Qt 6 Image edge-case compatibility fixture. It is run by
  `mise run script-smoke-image-edges-qt6` and verifies empty image metadata and
  save errors, constructor argument-count errors, non-path argument rejection,
  missing-image load errors that clear the image handle, incompatible
  raster/ToonzRaster save rejection, and unrecognized output type errors.
- `toonz/sources/tests/scriptengine/image_lifecycle_edges.toonzscript` is the
  repo-local Qt 6 Image lifecycle compatibility fixture. It is run by
  `mise run script-smoke-image-lifecycle-edges-qt6` and verifies loaded raster
  image metadata before disposal, disposed id reporting, and post-dispose
  rejection for `toString`, metadata access, `load()`, and `save()`.
- `toonz/sources/tests/scriptengine/image_level_first_frame.toonzscript` is the
  repo-local Qt 6 Image level-loading compatibility fixture. It is run by
  `mise run script-smoke-image-level-first-frame-qt6` and verifies
  `Image.load()`/`new Image(path)` on a saved two-frame Raster image sequence,
  including the legacy `Loaded first frame of 2` warning, implicit first-frame
  fallback, explicit frame-2 path loading, Raster type reporting, size, DPI,
  and string reporting.
- `toonz/sources/tests/scriptengine/image_builder.toonzscript` is the first
  repo-local Qt 6 `Transform`/`ImageBuilder` compatibility fixture. It is run
  by `mise run script-smoke-image-builder-qt6` and verifies identity,
  translation, rotation, uniform scale, non-uniform scale, transform string
  reporting, translated raster composition, generated image access, image save
  and reload, clear/fill behavior, and typed Raster/ToonzRaster builder
  construction.
- `toonz/sources/tests/scriptengine/image_builder_edges.toonzscript` is the
  repo-local Qt 6 ImageBuilder edge-case compatibility fixture. It is run by
  `mise run script-smoke-image-builder-edges-qt6` and verifies constructor
  argument-count/type/size errors, bad image type rejection, invalid fill color,
  empty-image add errors, non-`Transform` add argument rejection, ToonzRaster
  fill rejection, image type mismatch errors, and disposed builder rejection
  for `toString()`, `image`, `clear()`, `fill()`, and `add()`.
- `toonz/sources/tests/scriptengine/transform_edges.toonzscript` is the
  repo-local Qt 6 Transform edge-case compatibility fixture. It is run by
  `mise run script-smoke-transform-edges-qt6` and verifies finite-number
  argument rejection for `translate()`, `rotate()`, and `scale()`, plus
  disposed `toString()`, `translate()`, `rotate()`, and `scale()` rejection
  without entering rendering paths.
- `toonz/sources/tests/scriptengine/toonz_raster_converter.toonzscript` is the
  first repo-local Qt 6 converter compatibility fixture. It is run by
  `mise run script-smoke-toonz-raster-converter-qt6` and verifies
  `ToonzRasterConverter` construction, static raster-image conversion to a
  ToonzRaster `Image`, TLV save/reload, and existing compatibility helpers such
  as `toString()`, `foo()`, and `flatSource`.
- `toonz/sources/tests/scriptengine/toonz_raster_converter_level.toonzscript`
  is the repo-local Qt 6 level-wide converter compatibility fixture. It is run
  by `mise run script-smoke-toonz-raster-converter-level-qt6` and verifies
  instance and static `ToonzRasterConverter.convert(Level)` behavior by
  converting a two-frame raster level to a ToonzRaster level, reading converted
  frames, saving the converted TLV, and reloading a frame from disk.
- `toonz/sources/tests/scriptengine/toonz_raster_converter_edges.toonzscript`
  is the repo-local Qt 6 converter edge-case fixture. It is run by
  `mise run script-smoke-toonz-raster-converter-edges-qt6` and verifies
  legacy-style `ToonzRasterConverter.convert()` argument and type rejection for
  missing/extra arguments, non-image/non-level values, ToonzRaster and Vector
  images, empty Raster levels, and Vector levels.
- `toonz/sources/tests/scriptengine/toonz_raster_converter_lifecycle_edges.toonzscript`
  is the repo-local Qt 6 converter lifecycle fixture. It is run by
  `mise run script-smoke-toonz-raster-converter-lifecycle-edges-qt6` and
  verifies instance `convert()` rejection after `dispose()`, disposed id
  reporting, stable `toString()` behavior, and continued static conversion.
- `toonz/sources/tests/scriptengine/outline_vectorizer.toonzscript` is the
  first repo-local Qt 6 vectorizer compatibility fixture. It is run by
  `mise run script-smoke-outline-vectorizer-qt6` and verifies
  `OutlineVectorizer` construction, property get/set behavior, raster-image
  vectorization to a Vector `Image`, PLI save/reload, and inserting the
  vectorized image into a vector `Level`.
- `toonz/sources/tests/scriptengine/centerline_vectorizer.toonzscript` is the
  second repo-local Qt 6 vectorizer compatibility fixture. It is run by
  `mise run script-smoke-centerline-vectorizer-qt6` and verifies
  `CenterlineVectorizer` construction, property get/set behavior, raster-image
  vectorization to a Vector `Image`, PLI save/reload, and inserting the
  vectorized image into a vector `Level`.
- `toonz/sources/tests/scriptengine/vectorizer_edges.toonzscript` is the
  repo-local Qt 6 vectorizer edge-case fixture. It is run by
  `mise run script-smoke-vectorizer-edges-qt6` and verifies legacy-style
  `OutlineVectorizer.vectorize()` and `CenterlineVectorizer.vectorize()`
  argument and type rejection for non-image/non-level values, Vector images,
  empty Raster levels, and Vector levels without entering renderer paths.
- `toonz/sources/tests/scriptengine/binding_lifecycle_edges.toonzscript` is a
  repo-local Qt 6 non-rendering binding lifecycle/property fixture. It is run by
  `mise run script-smoke-binding-lifecycle-edges-qt6` and verifies property
  get/set roundtrips for `OutlineVectorizer`, `CenterlineVectorizer`, and
  `Rasterizer`, invalid `OutlineVectorizer.transparentColor` rejection, and
  disposed-object method/property rejection for `OutlineVectorizer`,
  `CenterlineVectorizer`, `Rasterizer`, and `ToonzRasterConverter`, including
  disposed `vectorize()`, `rasterize()`, and converter instance `convert()`
  calls.
- `toonz/sources/tests/scriptengine/rasterizer.toonzscript` is the repo-local
  Qt 6 Rasterizer compatibility fixture. It is run by
  `mise run script-smoke-rasterizer-qt6` and verifies `Rasterizer`
  construction, property get/set behavior, color-mapped vector-image
  rasterization to a ToonzRaster `Image`, TLV save/reload, inserting the
  rasterized image into a ToonzRaster `Level`, and full-color vector
  image/level rasterization through `TOfflineGL` to Raster `Image`/`Level`
  output. This fixture opts into `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` because
  the full-color path needs Qt's plugin/offscreen-surface path.
- `toonz/sources/tests/scriptengine/rasterizer_edges.toonzscript` is the
  repo-local Qt 6 Rasterizer edge-case fixture. It is run by
  `mise run script-smoke-rasterizer-edges-qt6` and verifies legacy-style
  `Rasterizer.rasterize()` argument and type rejection for non-image/non-level
  values, Raster/ToonzRaster images, Raster/Empty levels, and bad full-color
  resolution/DPI rejection while keeping the color-mapped path green.
- `toonz/sources/tests/scriptengine/renderer_basic.toonzscript` is the
  repo-local Qt 6 Renderer compatibility fixture. It is run by
  `mise run script-smoke-renderer-qt6` and verifies `Renderer` construction,
  read-only `id`, mutable `frames`/`columns` arrays, `toString()`,
  `renderFrame()`, `renderScene()`, and disposal. The fixture creates a
  scene-owned Raster level, renders one frame, saves/reloads the rendered PNG,
  renders a one-frame output level, and validates the Raster image/level shape.
  This smoke opts into `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` because renderer
  execution needs Qt's plugin/offscreen-surface path. It also verifies that
  `dumpCache()` writes a portable cache-map diagnostic under the configured
  OpenToonz cache root and returns the generated `FilePath`.
- `toonz/sources/tests/scriptengine/renderer_frames_columns.toonzscript` is the
  repo-local Qt 6 Renderer frame/column selection fixture. It is run by
  `mise run script-smoke-renderer-frames-columns-qt6` and verifies that
  `Renderer.frames` and `Renderer.columns` drive actual
  `renderScene()`/`renderFrame()` execution for a two-row, two-column Raster
  scene, including selected-column multi-frame output and default full-scene
  frame-list behavior. This smoke also opts into
  `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1`.
- `toonz/sources/tests/scriptengine/renderer_vector.toonzscript` is the
  repo-local Qt 6 Renderer vector-input fixture. It is run by
  `mise run script-smoke-renderer-vector-qt6` and verifies that a scene-owned
  Vector level can pass through `Renderer.renderFrame()` and
  `Renderer.renderScene()` under the Qt 6 `QJSEngine` facade, producing Raster
  image/level output that can be saved and reloaded. This smoke also opts into
  `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1`.
- `toonz/sources/tests/scriptengine/renderer_edges.toonzscript` is the
  repo-local Qt 6 Renderer edge-case compatibility fixture. It is run by
  `mise run script-smoke-renderer-edges-qt6` and verifies that
  `renderScene()` and `renderFrame()` reject missing/non-`Scene`/disposed scene
  arguments, bad frame values, and invalid `Renderer.frames` /
  `Renderer.columns` list values before reaching the Qt 6 renderer path.
- `toonz/sources/tests/scriptengine/renderer_lifecycle_edges.toonzscript` is
  the repo-local Qt 6 Renderer lifecycle compatibility fixture. It is run by
  `mise run script-smoke-renderer-lifecycle-edges-qt6` and verifies that
  `Renderer.toString()`, `renderScene()`, `renderFrame()`, and `dumpCache()`
  reject use after `Renderer.dispose()` while preserving script-owned
  `frames` and `columns` arrays.
- `toonz/sources/tests/scriptengine/wrapper_id.toonzscript` is the repo-local
  Qt 6 inherited Wrapper id compatibility fixture. It is run by
  `mise run script-smoke-wrapper-id-qt6` and verifies read-only `id` parity for
  the non-rendering `QJSEngine` facades: `FilePath`, `Scene`, `Level`, `Image`,
  `Transform`, `ImageBuilder`, `ToonzRasterConverter`, `OutlineVectorizer`,
  `CenterlineVectorizer`, `Rasterizer`, and `Renderer`, including disposal-time
  `-1` ids for disposable facade objects.
- `toonz/sources/tests/scriptengine/level_path.toonzscript` is the repo-local
  Qt 6 `Level.path` compatibility fixture. It is run by
  `mise run script-smoke-level-path-qt6` and verifies setter parity for both
  `FilePath` objects and strings by assigning a saved raster level path and
  reloading that level through the Qt 6 facade.
- `toonz/sources/tests/scriptengine/level_path_edges.toonzscript` extends
  `Level.path` coverage. It is run by
  `mise run script-smoke-level-path-edges-qt6` and verifies bad path setter
  rejection plus the current failed path reload behavior, where a missing
  target path assignment leaves the wrapper as a zero-frame level at the
  requested path.
- `toonz/sources/tests/scriptengine/level_lifecycle_edges.toonzscript` is the
  repo-local Qt 6 Level lifecycle fixture. It is run by
  `mise run script-smoke-level-lifecycle-edges-qt6` and verifies promoted
  empty-level metadata before disposal plus post-dispose rejection for string
  conversion, metadata/path access and mutation, frame access/mutation,
  `load()`, and `save()`.
- `toonz/sources/tests/scriptengine/level_transformers.toonzscript` is the
  repo-local Qt 6 level-wide transformer compatibility fixture. It is run by
  `mise run script-smoke-level-transformers-qt6` and verifies
  `OutlineVectorizer.vectorize(Level)`, `CenterlineVectorizer.vectorize(Level)`,
  and `Rasterizer.rasterize(Level)`.
- The script fixtures can run in bounded mode or natural-exit mode.
  `mise run script-smoke-qt6`, `mise run script-smoke-run-errors-qt6`,
  `mise run script-smoke-filepath-qt6`,
  `mise run script-smoke-filepath-coercion-qt6`,
  `mise run script-smoke-filepath-edges-qt6`,
  `mise run script-smoke-filepath-metadata-qt6`,
  `mise run script-smoke-filepath-mutation-metadata-qt6`,
  `mise run script-smoke-path-arguments-qt6`,
  `mise run script-smoke-scene-qt6`, `mise run script-smoke-scene-cells-qt6`,
  `mise run script-smoke-scene-columns-qt6`,
  `mise run script-smoke-scene-cell-fids-qt6`,
  `mise run script-smoke-scene-edges-qt6`,
  `mise run script-smoke-scene-argument-edges-qt6`,
  `mise run script-smoke-scene-lifecycle-edges-qt6`,
  `mise run script-smoke-scene-level-wrappers-qt6`,
  `mise run script-smoke-scene-loadlevel-qt6`,
  `mise run script-smoke-scene-loadlevel-sequence-qt6`,
  `mise run script-smoke-scene-save-reopen-qt6`,
  `mise run script-smoke-scene-reload-edges-qt6`,
  `mise run script-smoke-scene-load-failure-qt6`,
  `mise run script-smoke-scene-save-icon-qt6`,
  `mise run script-smoke-scene-save-icon-variants-qt6`,
  `mise run script-smoke-scene-frameids-qt6`,
  `mise run script-smoke-level-qt6`, `mise run script-smoke-level-edges-qt6`,
  `mise run script-smoke-level-io-qt6`,
  `mise run script-smoke-level-io-types-qt6`,
  `mise run script-smoke-level-path-qt6`,
  `mise run script-smoke-level-path-edges-qt6`,
  `mise run script-smoke-level-lifecycle-edges-qt6`,
  `mise run script-smoke-image-qt6`,
  `mise run script-smoke-image-edges-qt6`,
  `mise run script-smoke-image-lifecycle-edges-qt6`,
  `mise run script-smoke-image-level-first-frame-qt6`,
  `mise run script-smoke-image-builder-qt6`,
  `mise run script-smoke-image-builder-edges-qt6`,
  `mise run script-smoke-transform-edges-qt6`,
  `mise run script-smoke-toonz-raster-converter-qt6`,
  `mise run script-smoke-toonz-raster-converter-level-qt6`,
  `mise run script-smoke-toonz-raster-converter-edges-qt6`,
  `mise run script-smoke-toonz-raster-converter-lifecycle-edges-qt6`,
  `mise run script-smoke-outline-vectorizer-qt6`,
  `mise run script-smoke-centerline-vectorizer-qt6`,
  `mise run script-smoke-vectorizer-edges-qt6`,
  `mise run script-smoke-binding-lifecycle-edges-qt6`,
  `mise run script-smoke-rasterizer-qt6`,
  `mise run script-smoke-rasterizer-edges-qt6`,
  `mise run script-smoke-renderer-qt6`,
  `mise run script-smoke-renderer-frames-columns-qt6`,
  `mise run script-smoke-renderer-vector-qt6`,
  `mise run script-smoke-renderer-edges-qt6`,
  `mise run script-smoke-renderer-lifecycle-edges-qt6`,
  `mise run script-smoke-wrapper-id-qt6`, and
  `mise run script-smoke-level-transformers-qt6` pass in both modes for the
  current Qt 6 app bundle.
- `mise run script-smokes-qt6` now runs every current Qt 6 script fixture in
  bounded mode, and `mise run script-smokes-natural-exit-qt6` runs the same
  fixture set with `OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1`. Most fixtures use
  the fast QCoreApplication script path; the renderer and full-color Rasterizer
  fixtures opt into `QApplication` because rendering needs Qt plugins/offscreen
  surfaces. Use the aggregate tasks as the default script-parity regression gate
  after adding or changing a QJSEngine facade fixture, and keep the individual
  fixture tasks for focused failure triage. On June 5, 2026,
  `mise run script-smokes-qt6` and `mise run script-smokes-natural-exit-qt6`
  both passed against the current Qt 6 app bundle, including the
  QApplication-backed Rasterizer and Renderer fixtures.
- On 2026-06-04, focused scene-icon verification passed under
  `QT_QPA_PLATFORM=offscreen` for `mise run script-smoke-scene-save-icon-qt6`
  and `mise run script-smoke-scene-save-icon-variants-qt6`. A broader forced
  offscreen `mise run script-smokes-qt6` run still fails at
  `script-smoke-rasterizer-qt6` after the color-mapped Rasterizer checks, when
  full-color Rasterizer output enters `TOfflineGL` and Qt reports that the
  offscreen platform cannot create a platform OpenGL context. `QtOfflineGL` now
  validates offscreen surface, context, current-context, and framebuffer
  creation so this unsupported path exits quickly with `Rasterization failed`
  instead of timing out in GL setup. The remaining full-color
  Rasterizer/Renderer offscreen OpenGL path needs either a real OpenGL-capable
  QApplication validation run or a product-appropriate non-OpenGL
  implementation; it is not closed by the scene-icon fallback.
- Qt 6 command-line script execution now uses an early headless path before
  macOS `QApplication`, plugin loading, and main-window construction. That path
  still performs Toonz environment setup, uses `stuff/cache` for script-mode
  cache writes, and reports `TException`/`std::exception` failures to stderr.
- Qt 6 script execution can opt into the normal `QApplication` startup path with
  `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1`. On macOS that path restores the launch
  working directory before running the script, after the bundle-relative startup
  work has completed, so existing relative fixture paths keep the same meaning
  as the fast QCoreApplication path.
- The Qt 6 script-smoke harness now creates a smoke-local `SystemVar.ini`,
  passes explicit Toonz path qualifiers, and launches from the smoke root with a
  `toonz` symlink back to the checkout. This keeps profile/config/cache writes
  under `toonz/build/qt6-script-smoke` instead of creating repo-root `profiles`
  files while preserving existing relative fixture paths. Unix `TEnv` startup
  now honors explicit process environment values before the legacy
  `SystemVar.ini` warning fallback, and the harness exports the same isolated
  Toonz paths before launch.
- The Qt 6 Script Console now installs a GUI-only `view()` bridge for `Image`
  and `Level` values on top of the `QJSEngine` facade. The helper stays in the
  `toonz` app layer so flipbook UI dependencies do not leak into `toonzlib`;
  `mise run gui-smoke-script-console-view-qt6` validates the GUI helper against
  generated `Image` and `Level` values, warning output delivery, a repeated
  command after the `view()` calls, `run()` of a child script from the isolated
  library scripts folder, child-script print output, child-script return
  values, child-script global variable persistence back to the console,
  expected error display for invalid `view()` usage, and one Up-arrow
  command-history recall through the real console widget. Headless script-smoke
  execution still does not expose `view()`, and manual console verification
  remains required for broader interactive command editing, `run()` error
  paths, FlipBook cleanup, repeated sessions, and real user scripts.
- On macOS, the packaged Qt 6 GUI smoke wrapper now launches the `.app` through
  LaunchServices by default. This avoids a sandbox-sensitive direct-executable
  stall inside `QApplication` construction and matches normal bundle startup
  more closely. `OPENTOONZ_GUI_SMOKE_DIRECT_EXEC=1` remains available for
  focused direct-launch diagnostics.
- The Qt 6 script smoke wrapper now uses LaunchServices for macOS
  `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` fixtures, while preserving direct
  execution for fast headless fixtures. It passes an explicit
  `OPENTOONZ_SCRIPT_WORKING_DIRECTORY`, which OpenToonz restores before running
  script bootstrap and fixture code. With this path, the aggregate Qt 6 script
  smoke suite passes, including full-color Rasterizer and Renderer
  QApplication fixtures.
- The Qt 6 branch now has a minimal `FilePath` compatibility slice for
  construction, string conversion, name/extension/parent accessors, mutation
  helpers, path concatenation, existence checks, directory checks, modified-time
  lookup as a JS `Date`, directory listing, absolute-path concat rejection, and
  non-directory `files()` errors. The shared path-argument checks now reject non-string,
  non-FilePath values for `run()`, FilePath parent/concat helpers, and
  Image/Level/Scene path methods instead of silently coercing arbitrary values
  through `String()`. This does not yet claim full `scriptbinding_files.*`
  parity.
- The Qt 6 branch now has a minimal `Scene` compatibility slice for
  construction, frame/column count, string conversion, column insert/delete,
  `setCell()`, `getCell()`, `loadLevel()`, numeric/string
  `Scene.getCell().fid` type parity, lettered frame-id cells, non-string
  `setCell()` level argument rejection, four-argument undefined level
  rejection, headless scene data save/reopen, existing-scene reload with stale
  level-wrapper invalidation, and a narrow
  `QApplication` script-mode scene-icon generation check. The Qt 6 script save
  path intentionally skips `makeSceneIcon()` for headless script execution, but
  opts into scene-icon generation when
  `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` is set so the offscreen-rendering
  boundary can be validated in the packaged app. This proves icon creation and
  image reload, not scene-icon visual parity, viewer parity, or broader
  render-output parity.
- The Qt 6 branch now has a minimal `Level` compatibility slice for empty
  handles, simple-level handles owned by a `Scene`, type/name/path/frame-count
  inspection, frame-id listing, name mutation, and frame get/set support for
  scene-owned and standalone levels when paired with the Qt 6 `Image` facade.
  `Level.load()`, `Level.save()`, and the `Level.path` setter now cover the
  first raster-level I/O and reload path through owned script scenes, plus
  Vector level frame reload and ToonzRaster level metadata/frame reload. The
  Level edge-case fixture also covers empty-level errors, missing-frame
  behavior, bad frame/index input including non-number frame-index rejection,
  empty-level name setter no-op parity, empty-level path `undefined` parity,
  level/image type mismatch, incompatible save, and missing-load errors.
  The Level lifecycle fixture covers promoted-empty-level metadata and
  post-dispose rejection for the Level public surface.
  Broader path-policy coverage still needs fixtures before claiming full
  `scriptbinding_level.*` parity.
- The Qt 6 branch now has a minimal `Image` compatibility slice for empty image
  handles, image load/save, type/size/DPI/string reporting, and use as a
  `Level.setFrame()` input. The Image edge-case fixture also covers empty-image
  save errors, constructor argument errors, non-path argument rejection, failed
  load state clearing, incompatible save errors, and unrecognized output type
  errors. The Image lifecycle edge-case fixture covers post-dispose rejection
  for metadata access, string conversion, load, and save calls. The Image
  level-first-frame fixture covers loading a saved two-frame Raster image
  sequence through `Image`, preserving the legacy first-frame warning while
  also checking explicit frame-2 path loading and observable Raster image
  metadata.
- The Qt 6 branch now has a minimal `Transform`/`ImageBuilder` compatibility
  slice for identity, translation, rotation, uniform scale, non-uniform scale,
  transform-derived raster composition, generated image access, clear/fill,
  image save/reload, and typed Raster/ToonzRaster builder construction. The
  Transform and ImageBuilder edge-case fixtures also cover invalid Transform
  numeric inputs, disposed Transform `toString()`, `translate()`, `rotate()`,
  and `scale()` rejection, constructor argument errors, invalid fill/add
  inputs, type mismatch rejection, ToonzRaster fill rejection, and disposed
  builder rejection for `toString()`, `image`, `clear()`, `fill()`, and
  `add()`.
- The Qt 6 branch now has a minimal `ToonzRasterConverter` compatibility slice
  for raster image conversion into ToonzRaster images, level-wide raster to
  ToonzRaster conversion, TLV save/reload, converted-level frame reload, and
  legacy-style argument/type rejection for unsupported converter inputs. It now
  also covers instance converter disposal while preserving static conversion
  behavior. This does not yet claim broader converter or renderer parity.
- The Qt 6 branch now has a minimal `OutlineVectorizer` compatibility slice for
  property state, raster-image vectorization into Vector images, PLI
  save/reload, vector-level insertion, and level-wide raster-to-vector
  conversion. It now also rejects unsupported argument and level/image types
  before attempting conversion. This does not yet claim full vectorizer,
  rasterizer visual parity, or renderer parity.
- The Qt 6 branch now has a minimal `CenterlineVectorizer` compatibility slice
  for property state, raster-image vectorization into Vector images, PLI
  save/reload, vector-level insertion, and level-wide raster-to-vector
  conversion. It now also rejects unsupported argument and level/image types
  before attempting conversion. This does not yet claim full vectorizer,
  rasterizer visual parity, or renderer parity.
- The Qt 6 branch now has a minimal `Rasterizer` compatibility slice for
  property state and color-mapped vector-image rasterization into ToonzRaster
  images, plus TLV save/reload, ToonzRaster level insertion, level-wide
  Vector-to-ToonzRaster conversion, and fixture-sized full-color vector
  image/level output through `TOfflineGL` to Raster `Image`/`Level` output. It
  now also rejects unsupported argument and level/image types before attempting
  rasterization. This is a narrow script/offscreen coverage slice, not viewer,
  final-render, or broader visual parity.
- The Qt 6 branch now has a `Renderer` compatibility slice for construction,
  `id`, `frames`/`columns` arrays, `toString()`, `renderFrame()`,
  `renderScene()`, and disposal. `renderFrame()` and `renderScene()` use the
  same `TRenderer`/`TRenderPort` shape as the Qt 5 binding and are covered by a
  QApplication script smoke that renders a scene-owned Raster level and reloads
  the saved output image. `dumpCache()` now writes `TImageCache`'s cache-map
  diagnostic to the configured cache root and returns the generated `FilePath`
  for script-side inspection. The renderer edge fixture also validates
  `Renderer.frames` and `Renderer.columns` list type/range errors before the
  renderer path starts.
- The Qt 6 branch now exposes the inherited `Wrapper::id` compatibility
  property on the non-rendering `QJSEngine` facades. This preserves a small but
  script-visible Qt 5 wrapper contract without touching renderer or viewer
  code.
- The remaining OpenToonz object binding groups are still largely Qt 5-only in
  this branch: broader rasterizer visual parity, broader converters,
  broader scene icon/rendering visual parity, renderer diagnostics beyond cache
  map dumping, and deeper scene/level APIs. Scene,
  Scene cell frame-id type, Scene lifecycle, Level, Image,
  Image level-first-frame, ImageBuilder, ImageBuilder edge-case,
  Transform edge-case, ToonzRasterConverter,
  OutlineVectorizer,
  CenterlineVectorizer, Rasterizer, and Renderer support are still partial
  compatibility slices
  even with the level-wide transformer, level-wide converter, scene data
  save/reopen, scene reload edge-case, scene edge-case, Renderer
  renderFrame/renderScene, Renderer frame/column selection, Renderer vector
  input, and Renderer edge-case fixtures. This is an explicit temporary
  limitation, not
  product-ready Qt 6 script support.

Avoid making an external Qt Script fork the default plan. It might be useful as
a short-lived proof-of-build bridge if maintainers explicitly accept the
maintenance cost, but it would keep the project pinned to an unmaintained API
shape.

### 4. Qt Multimedia, Audio, And Stop-Motion Capture

Qt 6 Multimedia was significantly refactored. The local areas touched include:

- `toonz/audiorecordingpopup.cpp`
- `toonz/autolipsyncpopup.cpp`
- `toonz/penciltestpopup.cpp`
- `toonz/penciltestpopup_qt.cpp`
- `stopmotion/*`
- `common/tsound/tsound_qt.cpp`

Expected API moves:

- `QCameraInfo` becomes `QMediaDevices` plus `QCameraDevice`.
- `QCameraImageCapture` becomes `QImageCapture`.
- `QCameraViewfinderSettings` becomes camera-format selection via
  `QCameraFormat`.
- `QAbstractVideoSurface` and `QVideoSurfaceFormat` should move to
  `QVideoSink` and Qt 6 video-frame handling.
- Raw audio recording paths should evaluate `QAudioSource`.
- Raw audio playback paths should evaluate `QAudioSink`.
- `QMediaPlayer` needs explicit audio output wiring through `QAudioOutput`.
- Recording through capture sessions uses `QMediaCaptureSession`,
  `QAudioInput`, `QCamera`, and `QMediaRecorder`.

This work cannot be considered done by compilation alone. It needs real
hardware smoke tests:

- built-in or USB camera capture
- stop-motion live preview
- still capture into a level
- audio recording
- audio playback and lip-sync preview
- device hotplug or refresh behavior where supported

Current branch status:

- The Qt 6 app target compiles with the refactored audio playback, audio
  recording, auto lip-sync playback, active pencil-test camera, and stop-motion
  camera enumeration code paths.
- `stopmotion/stopmotioncamera.h` centralizes the Qt 5 `QCameraInfo` versus Qt
  6 `QCameraDevice` split for camera descriptions, device ids, and supported
  resolutions.
- The legacy `penciltestpopup_qt.*` path remains a Qt 5-era 32-bit path and
  still contains `QAbstractVideoSurface`/`QCameraInfo` usage. It is not part of
  the current 64-bit Qt 6 app compile frontier.
- `mise run gui-smoke-media-devices-qt6` now provides a packaged-app
  enumeration check for the active Qt Multimedia API lane, audio input/output
  counts, and camera counts. This is useful preflight evidence before hardware
  testing, but it does not certify audio capture/playback or camera preview and
  still capture behavior.
- `mise run gui-smoke-camera-formats-qt6` now provides a green packaged-app Qt 6
  default camera-format enumeration check. It records the format count,
  resolution bounds, frame-rate bounds, and compact pixel-format metadata
  through `QCameraDevice` without constructing or starting a capture session.
  The current local run found one video input, the FaceTime HD Camera as
  default, seven default-camera formats, 640x480 to 1920x1920 resolution bounds,
  and 15-30 FPS frame-rate bounds. This is useful camera-backend metadata
  evidence, but it does not certify camera permission prompts, preview frames,
  still capture, hotplug behavior, or stop-motion UI workflows.
- `mise run gui-smoke-audio-output-qt6` now provides a packaged-app Qt 6 audio
  output backend startup check. It opens the default output with a short silent
  buffer and records the selected format, backend state, and error status. This
  is useful playback-backend evidence, but it does not certify audible playback
  fidelity, audio recording, lip-sync preview, or microphone permission flows.
- `mise run gui-smoke-audio-input-qt6` now provides a packaged-app Qt 6 audio
  input backend capture check. It opens the default input with `QAudioSource`,
  records the selected input and format, captures a short buffer, and verifies
  `NoError`. The current local run used the MacBook Pro Microphone at 48 kHz
  and captured 96,256 bytes. This is useful recording-backend evidence, but it
  does not certify the Record Audio popup, WAV writing, permission UX, lip-sync
  timing, or noisy-room audio quality.
- `mise run gui-smoke-audio-recording-wav-qt6` now provides a packaged-app
  Qt 6 capture-to-WAV check through the existing Record Audio writer. It opens
  the default input, records into `AudioWriterWAV`, saves a short WAV beside
  the GUI smoke status file, and reloads it through `TSoundTrackReader`. The
  current local run used the MacBook Pro Microphone at 44.1 kHz, recorded
  65,536 bytes, produced a 65,580-byte WAV, and reloaded 32,768 samples over
  743 ms. This is useful product-path evidence for the WAV writer and sound I/O
  reload, but it does not certify the Record Audio popup button flow, Save and
  Insert column insertion, microphone permission UX, lip-sync timing, or capture
  quality.
- `mise run gui-smoke-audio-playback-wav-qt6` now provides a packaged-app
  Qt 6 generated-WAV playback check through the product sound output
  abstraction. It writes a low-volume PCM16 WAV with `AudioWriterWAV`, reloads
  it through `TSoundTrackReader`, opens the default output through
  `TSoundOutputDevice`, and verifies playback starts and stops. The current
  local run used MacBook Pro Speakers at 44.1 kHz mono, wrote 264,600 bytes to
  a 264,644-byte WAV, and reloaded 132,300 samples over 3,000 ms. This is useful
  product-path evidence for generated WAV playback startup, but it does not
  certify audible output quality, speaker routing, flipbook/timeline playback,
  lip-sync preview timing, or Record Audio UI behavior.
- Product-level camera and audio hardware workflow tests have not been
  completed. Default audio input/output backend, capture-to-WAV reload,
  generated-WAV playback startup, and camera metadata smokes are green, but
  preview/capture/record/playback UI behavior is not product-certified.

### 5. OpenGL, Viewer, Offscreen Rendering, And Visual Parity

Qt 6 does not require removing OpenGL from a Qt Widgets application, but it
changes the module split and platform assumptions:

- `QOpenGLWidget` lives in the Qt OpenGL Widgets module.
- many `QOpenGL*` classes moved into Qt OpenGL.
- Windows no longer gets Qt 5's ANGLE fallback behavior from the old stack.
- high-DPI behavior is always enabled and the default rounding policy changed.

This checkout has a broad OpenGL surface:

- viewer widgets such as `sceneviewer`, `imageviewer`, and `planeviewer`
- offscreen render paths such as `qtofflinegl.cpp`
- core vector/raster render helpers under `common/tvrender`
- standard FX shader paths
- tool drawing overlays and selection rendering
- texture and GL utility code

The Metal draft is no longer a blocker for this port. Qt 6 rendering work
should proceed against the current OpenToonz visual behavior and keep Qt 5
parity visible while the new lane stabilizes. The safer plan is:

1. Use the Qt 5 application behavior as the visual and workflow baseline.
2. Fix Qt 6 widget integration, event handling, high-DPI, OpenGLWidgets,
   offscreen GL, shader, and tool-overlay behavior in narrow slices.
3. Keep OpenGL compatibility where it is still required by current OpenToonz
   rendering and non-macOS paths.
4. Revisit Metal after Qt 6 parity is complete, using the Qt 6-stabilized
   renderer behavior as the integration baseline.

Qt 6 rendering work is now allowed to touch files such as:

- `toonz/sources/toonz/sceneviewer.*`
- `toonz/sources/toonz/imageviewer.*`
- `toonz/sources/toonz/viewerdraw.cpp`
- `toonz/sources/common/tvrender/*gl*`
- `toonz/sources/common/tgl/*`
- `toonz/sources/include/tgl.h`
- `toonz/sources/include/qtofflinegl.h`
- `toonz/sources/stdfx/shader*`

Current branch status:

- The Qt 6 app target links while still using the existing OpenGL-heavy viewer
  and FX code. This proves the Qt 6 widget/build integration is viable, but it
  does not validate rendering behavior.
- The remaining OpenGL, offscreen rendering, shader, high-DPI, and
  tool-overlay work is now part of the active Qt 6 port rather than parked
  behind the Metal draft.
- An offscreen Qt 6 startup smoke is not a useful GUI substitute on macOS
  because the offscreen platform cannot create the OpenGL context OpenToonz
  still expects.
- A bounded Cocoa startup smoke with a copied `stuff` tree stayed alive until
  the smoke harness stopped it. That is a better first startup signal than the
  offscreen run.
- An earlier packaged Qt 6 interactive GUI smoke created a new scene in the
  isolated `gui-smoke-qt6` sandbox and then reopened that generated scene file
  through the command-line file argument. That validated the first scene
  creation/save/open path at the time and exposed the task-scheduler crash
  described below.
- The scene create/open smoke is repeatably green through the app-side status
  hook. `mise run gui-smoke-scene-create-qt6` now verifies scene creation and
  save without relying on the macOS accessibility layer, and
  `OPENTOONZ_GUI_SMOKE_FILE=<path-to-scene.tnz> bash scripts/qt6/run-gui-smoke.sh`
  verifies reopening the generated scene. System Events remains a fallback path
  in the harness, but it is not the authoritative Qt 6 create/open check because
  it can report zero OpenToonz windows even when Qt QPA logging confirms Cocoa
  created the main window and Startup popup. In sandboxed agent runs,
  AppleScript error `-10827` on the scene-create path should be rerun with GUI
  automation permission before being treated as an app regression.
- The app-side smoke hook also has a green non-rendering xsheet state check via
  `mise run gui-smoke-xsheet-scrub-qt6`. It creates raster/vector levels,
  populates xsheet cells, switches frame and column state, saves the sandbox
  scene, and checks the resulting counts. It does not validate viewer drawing,
  redraw, OpenGL fallback behavior, high-DPI rendering, preview render, or
  final render.
- The app-side smoke hook also has green raster and vector viewer framebuffer
  checks via `mise run gui-smoke-viewer-render-qt6` and
  `mise run gui-smoke-viewer-vector-render-qt6`. It inserts either a red raster
  frame or a red vector stroke into a sandbox scene, captures the `SceneViewer`
  framebuffer before and after the update, requires nonzero changed and
  red-dominant pixel counts, and writes the before/after captures beside the
  smoke status file. These are narrow packaged-app rendering guards only;
  brush input, interactive vector drawing, timeline/UI onion workflow, overlay
  placement, manual FX Preview UI and broader FX preview workflows, broader
  final-render parity, and Qt 5-vs-Qt 6 visual parity remain open.
  The raster viewer-render check now also fails if the logical viewer size,
  OpenToonz device-pixel ratio, Qt DPR, screen DPR, and captured framebuffer
  size disagree. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, `771605` changed
  pixels, and `358756` red pixels.
- The app-side smoke hook also has a green onion-skin framebuffer check via
  `mise run gui-smoke-viewer-onion-skin-qt6`. It creates two transparent
  raster levels in one column, captures the current row with onion skin
  disabled, enables whole-scene mobile onion skin at back offset `-1`, and
  requires one back onion stage player, visible red current-frame pixels, and
  an onion-pixel increase in the `SceneViewer` framebuffer. The latest packaged
  Qt 6 and Qt 5 runs both reported logical size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, `45651` red pixels, and onion pixels `0 -> 42012`.
  This covers the app-side onion framebuffer path only; timeline onion marker
  UI, interactive onion toggles, custom onion colors, overlay placement, and
  full visual parity remain open.
- The app-side smoke hook also has a green row-area onion marker UI check via
  `mise run gui-smoke-viewer-onion-skin-rowarea-qt6`. It creates a three-frame
  raster scene, captures the xsheet row area, sends Qt mouse events to set one
  relative and one fixed onion marker through `RowArea`, double-clicks the
  current-row onion handle off and back on, and requires changed row-area
  pixels, high-DPI row-area capture, MOS/FOS marker state, enable/disable
  toggle state, visible onion pixels, and visible current-frame red pixels. The
  latest packaged Qt 6 and Qt 5 runs both reported logical size `994x819`, DPR
  `2` / `2.00`, framebuffer `1988x1638`, onion rows `0,2`, MOS count `1`, FOS
  count `1`, row-area changed pixels `516`, row-area non-background pixels
  `106702`, onion pixels `0 -> 42012`, `10455` changed viewer pixels, and
  `45651` red pixels. This covers app-side xsheet row-area onion marker
  creation and current-row enable/disable toggling through Qt widget events;
  real OS-level input delivery, broader timeline onion workflows, and full
  visual parity remain open.
- The app-side smoke hook also has a green row-area onion marker drag-range
  check via `mise run gui-smoke-viewer-onion-skin-rowarea-drag-qt6`. It creates
  a five-frame raster scene, captures the xsheet row area, sends a Qt mouse
  drag from the row-0 MOS handle to row 4 through `RowArea` and `XsheetViewer`
  drag-tool dispatch, and requires a four-marker MOS range, changed row-area
  pixels, high-DPI row-area capture, visible onion pixels, and visible
  current-frame red pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  current row `2`, onion rows `0,1,3,4`, MOS count `4`, FOS count `0`, MOS
  range `-2,2`, row-area drag event delivery `true`, row-area changed pixels
  `1076`, row-area non-background pixels `106700`, onion pixels `0 -> 71318`,
  `43180` changed viewer pixels, and `45651` red pixels. This covers app-side
  xsheet row-area onion marker drag ranges through Qt widget events; real
  OS-level input delivery, broader timeline onion workflows, and full visual
  parity remain open.
- The app-side smoke hook also has a green fixed onion marker drag-range check
  via `mise run gui-smoke-viewer-onion-skin-fixed-marker-drag-qt6`. It creates
  a six-frame raster scene, sets row `3` as the current frame, sends fixed
  marker drags through `RowArea` to add back FOS rows `0,1,2`, remove rows
  `1,2`, and add front FOS rows `5,4`, and requires fixed add/remove/front
  drag delivery, FOS add/remove/final counts, changed row-area pixels, high-DPI
  row-area capture, visible onion pixels, and visible current-frame red pixels.
  The latest packaged Qt 6 and Qt 5 runs both reported logical size `994x819`,
  DPR `2` / `2.00`, framebuffer `1988x1638`, current row `3`, onion rows
  `0,4,5`, MOS count `0`, FOS count `3`, fixed-marker counts add `3`, remove
  `1`, final `3`, fixed-drag event delivery `true/true/true`, row-area changed
  pixels `768`, row-area non-background pixels `105554`, onion pixels
  `0 -> 34969`, `5952` changed viewer pixels, and `45651` red pixels. This
  covers app-side fixed onion-marker add/remove/front range drags through Qt
  widget events; real OS-level input delivery, broader timeline onion
  workflows, and full visual parity remain open.
- The app-side smoke hook also has a green onion-skin context-menu command
  check via `mise run gui-smoke-viewer-onion-skin-context-menu-qt6`. It creates
  a five-frame raster scene, builds the onion-skin command menu through
  `OnioniSkinMaskGUI::addOnionSkinCommand`, triggers activate, deactivate,
  extend to scene, limit to level, clear fixed markers, clear relative markers,
  and clear all markers, and requires command-state, MOS/FOS marker-state,
  row-area high-DPI, changed row-area pixels, visible onion pixels, and visible
  current-frame red pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  current row `3`, onion rows `0,1,2`, MOS count `3`, FOS count `0`, MOS range
  `-3,-1`, activate/deactivate/extend/limit/clear fixed/clear relative/clear
  all command status `ok`, row-area changed pixels `820`, row-area
  non-background pixels `106716`, onion pixels `0 -> 91203`, `52610` changed
  viewer pixels, and `45651` red pixels. This covers app-side onion-skin
  context-menu command behavior through Qt actions; real OS-level
  right-click/menu delivery, broader timeline onion workflows, and full visual
  parity remain open.
- The app-side smoke hook also has a green custom onion color check via
  `mise run gui-smoke-viewer-onion-skin-custom-colors-qt6`. It sets custom
  back/front onion colors, creates black back/front raster frames so the onion
  tint is visible, keeps a red current frame, enables back and front mobile
  onion markers, and requires stored preference colors, row-area marker color,
  viewer framebuffer tint, changed viewer pixels, and visible current-frame red
  pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, custom colors back
  `44,92,224` and front `232,184,28`, viewer custom-color pixels back `54237`
  and front `32761`, row-area custom-color pixels back `3545` and front `915`,
  current row `2`, onion rows `1,3`, MOS count `2`, FOS count `0`, `86998`
  changed viewer pixels, and `57927` red pixels. This covers app-side custom
  onion color preference propagation, row-area marker color, and viewer
  framebuffer tinting; real OS-level input/menu delivery, broader timeline
  onion workflows, and full visual parity remain open.
- The app-side smoke hook also has a green xsheet/timeline orientation check
  via `mise run gui-smoke-viewer-onion-skin-orientations-qt6`. It creates a
  three-frame raster scene, normalizes the xsheet to `TopToBottom`, sends Qt
  row-area events for relative and fixed onion marker creation plus current-row
  off/on toggles, flips to `LeftToRight`, and repeats the same marker sequence.
  The latest packaged Qt 6 and Qt 5 runs both reported logical size `994x819`,
  DPR `2` / `2.00`, framebuffer `1988x1638`, orientation
  `TopToBottom -> LeftToRight`, vertical row-area changed pixels `516`,
  vertical row-area non-background pixels `106702`, horizontal row-area changed
  pixels `572`, horizontal row-area non-background pixels `14294`, onion rows
  `0,2`, MOS count `1`, FOS count `1`, onion pixels `0 -> 42012`, `32761`
  changed viewer pixels, and `45651` red pixels. This covers app-side onion
  marker creation and current-row toggles across both current xsheet/timeline
  orientations; real OS-level input/menu delivery, other timeline onion
  workflows, and full visual parity remain open.
- The app-side smoke hook also has a green Shift and Trace ghost-marker check
  via `mise run gui-smoke-viewer-onion-skin-shift-trace-qt6`. It creates a
  five-frame raster scene, enables Shift and Trace, clicks the row-area ghost
  markers to move the back ghost to row `0`, move the front ghost to row `4`,
  reset through the current row, hide both ghosts, and restore both final
  ghosts. The latest packaged Qt 6 and Qt 5 runs both reported logical size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, offsets initial
  `-1,1`, moved `-2,2`, reset `-1,1`, hidden `0,0`, final `-2,2`, move/reset
  and hide event delivery `true/true/true/true/true`, current row `2`, stage
  players `3`, stage onion players `2`, back/front onion players `1/1`, onion
  rows `0,4`, row-area changed pixels `2658`, row-area non-background pixels
  `105636`, onion pixels `33805 -> 44099`, `44864` changed viewer pixels, and
  `47307` red pixels. This covers app-side Shift and Trace ghost marker
  move/reset/hide behavior through Qt widget events; real OS-level input/menu
  delivery, additional timeline onion workflows, and full visual parity remain
  open.
- The app-side smoke hook also has a green camera-box overlay framebuffer check
  via `mise run gui-smoke-viewer-camera-overlay-qt6`. It creates a blank
  sandbox scene, toggles the camera box overlay off and on through
  `MI_ViewCamera` / `viewCameraToggle`, captures the `SceneViewer` framebuffer
  before and after the toggle, and requires `cameraOverlayProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, changed pixels, and red
  overlay pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, `3638` changed
  pixels, and `3638` red pixels. This covers the app-side camera-box overlay
  path only; selection handles, tool cursor overlays, rulers, OS/menu
  interaction, and full overlay visual parity remain open.
- The app-side smoke hook also has a green safe-area and field-guide overlay
  framebuffer check via `mise run gui-smoke-viewer-safe-area-field-guide-qt6`.
  It creates a blank sandbox scene, toggles safe area and field guide off and
  on through `MI_SafeArea` / `safeAreaToggle` and `MI_FieldGuide` /
  `fieldGuideToggle`, captures the `SceneViewer` framebuffer before and after
  the toggle, and requires `safeAreaProbe=ok`, `fieldGuideProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, changed pixels, red
  safe-area pixels, gray guide pixels, and changed gray guide pixels. The latest
  packaged Qt 6 and Qt 5 runs both reported logical size `994x819`, DPR `2` /
  `2.00`, framebuffer `1988x1638`, `112230` changed pixels, `5279` red pixels,
  `645999` gray pixels, and `106951` changed gray pixels. This covers the
  app-side safe-area and field-guide overlay paths only; selection handles, tool
  cursor overlays, rulers/guides added through ruler UI, OS/menu interaction,
  safe-area variants beyond the separate preset/color and custom-file smokes,
  field-guide
  variants beyond the separate size/aspect settings smoke, and full overlay
  visual parity remain open.
- The app-side smoke hook also has a green safe-area preset/color check via
  `mise run gui-smoke-viewer-safe-area-presets-qt6`. It creates a blank sandbox
  scene, disables camera, field-guide, ruler, guide, and safe-area overlays,
  captures the `SceneViewer`, enables the default `PR_safe` safe-area preset,
  switches to the bundled custom `150MT_FR_PR_safe` preset, and requires stored
  safe-area preset name, visible default red safe-area pixels, visible custom
  red/green/blue safe-area pixels, changed green and blue pixels between
  presets, and high-DPI framebuffer probes. The latest packaged Qt 6 and Qt 5
  runs both reported logical size `1006x831`, DPR `2` / `2.00`, framebuffer
  `2012x1662`, preset `PR_safe -> 150MT_FR_PR_safe`, default red pixels `5279`,
  default changed red pixels `5279`, custom red pixels `4884`, custom green
  pixels `2873`, custom blue pixels `3008`, preset-delta changed pixels
  `16043`, preset-delta green pixels `2873`, preset-delta blue pixels `3008`,
  and final red pixels `4884`. This covers app-side safe-area preset selection,
  bundled custom-color parsing, and viewer framebuffer rendering; real title-bar
  context-menu delivery, persistent user-managed safe-area file lifecycle,
  OS/menu interaction, and full overlay visual parity remain open.
- The app-side smoke hook also has a green user-authored safe-area file check
  via `mise run gui-smoke-viewer-safe-area-custom-file-qt6`. It creates a blank
  sandbox scene, disables camera, field-guide, ruler, guide, and safe-area
  overlays, writes a custom `safearea.ini` into the isolated smoke config root,
  enables the custom `codex_custom_safe_area_probe` preset, and requires
  custom-file write/restore, stored safe-area preset name, visible
  red/green/blue safe-area pixels, changed red/green/blue safe-area pixels, and
  high-DPI framebuffer probes. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `1006x831`, DPR `2` / `2.00`, framebuffer `2012x1662`,
  custom-file write and restore `true`, preset `codex_custom_safe_area_probe`,
  custom red pixels `2724`, custom green pixels `2248`, custom blue pixels
  `1712`, changed red pixels `2724`, changed green pixels `2248`, changed blue
  pixels `1712`, total changed pixels `6684`, and final red pixels `2724`.
  This covers app-side `safearea.ini` parsing and viewer framebuffer rendering
  inside the isolated smoke config root; real title-bar context-menu delivery,
  persistent user-profile file management outside the smoke root, OS/menu
  interaction, and full overlay visual parity remain open.
- The app-side smoke hook also has a green field-guide settings check via
  `mise run gui-smoke-viewer-field-guide-settings-qt6`. It creates a blank
  sandbox scene, disables camera, safe-area, field-guide, ruler, and guide
  overlays, captures the `SceneViewer`, applies field-guide size/aspect settings
  `10` / `1.3333` and then `24` / `2.4000`, and requires stored field-guide
  settings, visible gray guide pixels in both captures, changed gray guide pixels
  between settings, high-DPI framebuffer probes, and no red pixels. The latest
  packaged Qt 6 and Qt 5 runs both reported logical size `1006x831`, DPR `2` /
  `2.00`, framebuffer `2012x1662`, field-guide settings `16 -> 10 -> 24`,
  aspect `1.7778 -> 1.3333 -> 2.4000`, first gray pixels `619965`, first
  changed gray pixels `49276`, second gray pixels `699049`, second changed gray
  pixels `145718`, settings-delta changed pixels `141998`, settings-delta gray
  pixels `119220`, and `0` red pixels. This covers app-side field-guide
  size/aspect setting propagation and viewer framebuffer rendering; field-guide
  variants outside this size/aspect guard, OS/menu interaction, and full overlay
  visual parity remain open.
- The app-side smoke hook also has a green ruler/guide overlay framebuffer
  check via `mise run gui-smoke-viewer-ruler-guide-qt6`. It creates a blank
  sandbox scene, seeds one horizontal and one vertical scene guide, toggles the
  ruler and guide overlays through `MI_ViewRuler` / `viewRulerToggle` and
  `MI_ViewGuide` / `viewGuideToggle`, captures the `SceneViewer` after the
  ruler is visible but before guides are enabled, and then requires
  `rulerGuideProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
  two visible ruler widgets, guide counts, changed pixels, and changed neutral
  guide pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, `1707` changed
  pixels, `1707` changed neutral guide pixels, `551872` gray pixels, `0`
  changed gray pixels, and `0` red pixels. This covers the app-side guide
  framebuffer and ruler-widget visibility paths only; guide creation/movement
  through the ruler UI, ruler tick rendering pixels, selection handles, tool
  cursor overlays, OS/menu interaction, custom guide positions, and
  full overlay visual parity remain open.
- The app-side smoke hook also has a green ruler-guide event check via
  `mise run gui-smoke-viewer-ruler-guide-events-qt6`. It creates a blank
  sandbox scene, clears guides, enables the ruler and guide overlays, resolves
  the visible horizontal and vertical `Ruler` widgets, then sends Qt mouse
  events to create and move one horizontal and one vertical guide and
  right-click-delete the horizontal guide. It requires delivered ruler drag
  events, guide value changes, horizontal guide deletion, the vertical guide
  remaining, one final scene guide, a nonblank framebuffer, changed neutral
  guide pixels, and two visible ruler widgets. The latest packaged Qt 6 and Qt
  5 runs both reported logical size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, horizontal guides `0 -> 1 -> 0`, vertical guides `0 -> 1 -> 1`,
  horizontal guide value `-77.2788 -> 94.4181 -> deleted`, vertical guide value
  `67.8050 -> -103.8918`, `994` changed pixels, `994` changed neutral guide
  pixels, `539112` gray pixels, `0` changed gray pixels, and `0` red pixels.
  This covers app-side ruler-guide create, move, and delete through `Ruler` Qt
  mouse events; real OS-level input delivery, ruler tick rendering pixels,
  custom guide positions, ruler drag-hide variants, remaining manual
  ruler-guide variants, and full overlay visual parity remain open.
- The app-side smoke hook also has a green ruler-guide variant check via
  `mise run gui-smoke-viewer-ruler-guide-variants-qt6`. It creates a blank
  sandbox scene, clears guides, enables the ruler and guide overlays, resolves
  the visible horizontal and vertical `Ruler` widgets, creates horizontal and
  vertical guides through Qt mouse events, drags each one outside its ruler to
  exercise the drag-hide/delete path, and creates final horizontal and vertical
  guides for a framebuffer delta. It requires delivered create/hide/final drag
  events, guide counts returning to zero after each hide, two final scene
  guides, a nonblank framebuffer, changed neutral guide pixels, and two visible
  ruler widgets. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, horizontal hide
  counts `0 -> 1 -> 0 -> 1`, vertical hide counts `0 -> 1 -> 0 -> 1`, hide
  endpoints `H 617.00,-8.00` and `V -8.00,529.50`, `1697` changed pixels,
  `1697` changed neutral guide pixels, and `0` red pixels. This covers
  app-side ruler-guide drag-hide variants through `Ruler` Qt mouse events; real
  OS-level input delivery, manual ruler-guide variants not covered by app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, and full overlay visual parity remain
  open.
- The app-side smoke hook also has a green ruler guide-line rendering check via
  `mise run gui-smoke-viewer-ruler-guide-lines-qt6`. It creates a blank sandbox
  scene, seeds two horizontal and two vertical guides at custom world positions,
  captures the viewer with guides disabled, enables the guide overlay, moves
  the guides, and captures again. It requires a nonblank viewer framebuffer,
  high-DPI framebuffer probes, custom guide counts, changed neutral guide-line
  pixels, vertical and horizontal dashed guide-line segments, expected
  guide-position bands, moved-guide framebuffer deltas, and two visible ruler
  widgets. The latest packaged Qt 6 and Qt 5 runs both reported logical size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, guide positions H
  `-96.00,132.00 -> -156.00,48.00` and V
  `-88.00,116.00 -> -136.00,56.00`, first guide-line neutral pixels `3393`,
  second guide-line neutral pixels `3394`, two detected guide columns and two
  guide rows in each capture, dashed segment counts V/H `702/993` then
  `703/992`, moved guide-line neutral pixels `3681`, `6785` changed pixels,
  and `0` red pixels. This covers app-side custom guide positions and dashed
  viewer guide-line rendering; real OS-level input delivery, manual ruler-guide
  variants, guide-line visual variants outside this app-side dashed-line guard,
  and full overlay visual parity remain open.
- The app-side smoke hook also has a green ruler tick rendering check via
  `mise run gui-smoke-viewer-ruler-ticks-qt6`. It creates a visible raster
  scene, disables guide overlays, enables the ruler overlay, resolves the
  visible horizontal and vertical `Ruler` widgets, captures the viewer and both
  ruler widgets before and after a viewer transform change, and requires a
  nonblank viewer framebuffer, visible red content, high-DPI widget captures,
  nonzero ruler tick pixels, changed ruler tick pixels, changed ruler units,
  and two visible ruler widgets. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  horizontal ruler ticks `4098 -> 4074` with `224` changed tick pixels,
  vertical ruler ticks `3410 -> 3374` with `236` changed tick pixels, ruler
  units `124.2500 -> 180.1625` on both rulers, `1425551` changed viewer
  pixels, and `747490` red pixels. This covers app-side ruler widget tick
  rendering after viewer transform changes; real OS-level input delivery,
  remaining manual ruler-guide variants beyond app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, and full overlay visual parity remain
  open.
- The app-side smoke hook also has a green ruler/guide style check via
  `mise run gui-smoke-viewer-ruler-guide-styles-qt6`. It creates a blank
  sandbox scene, seeds one horizontal and one vertical guide, enables the ruler
  widgets, captures both `Ruler` widgets, applies custom QSS through the
  existing ruler qproperty style path, enables the guide overlay, and requires
  high-DPI framebuffer and widget captures, custom ruler background/handle/scale
  pixels, changed ruler widget pixels, visible ruler widgets, guide counts, and
  changed neutral guide pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  horizontal ruler style pixels background `0 -> 43570`, handle `0 -> 48`,
  scale `124`, changed `47712`, vertical ruler style pixels background
  `0 -> 35858`, handle `0 -> 48`, scale `136`, changed `39312`, `1697`
  changed neutral guide pixels, and `0` red pixels. This covers app-side ruler
  QSS/qproperty rendering and guide-overlay coexistence; real OS-level input
  delivery, remaining manual ruler-guide variants beyond app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, and full overlay visual parity remain
  open.
- The app-side smoke hook also has a green Animate/Edit tool overlay
  framebuffer check via `mise run gui-smoke-viewer-animate-tool-overlay-qt6`.
  It creates a visible raster scene, captures a `T_Hand` baseline, switches the
  active tool to `T_Edit` on `Col1`, and requires `toolOverlayProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, changed pixels, and
  visible red content pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  `2898` changed pixels, and `357248` red pixels. This covers the
  `SceneViewer::drawToolGadgets()` Animate/Edit overlay path only; selection
  handles, cursor feedback, interactive transform dragging, OS/menu
  interaction, and full overlay visual parity remain open.
- The app-side smoke hook also has a green Animate/Edit direct transform-drag
  check via `mise run gui-smoke-viewer-animate-tool-drag-qt6`. It creates a
  visible raster scene, activates `T_Edit` on `Col1`, captures the viewer with
  the tool active, replays a direct tool left-button drag from `0,0` to
  `144,-72`, and requires `toolTransformProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, changed `TStageObject` placement, changed
  pixels, and visible red content pixels. The latest packaged Qt 6 and Qt 5 runs
  both reported logical size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, stage object X `0.0000 -> 2.7000`, stage object Y
  `0.0000 -> -1.3500`, `492731` changed pixels, and `356145` red pixels. This
  covers the direct Animate/Edit tool transform path only; selection-handle
  hit-testing, cursor feedback, real OS-level or hardware input delivery, and
  full transform workflow visual parity remain open. Undo/redo integration and
  modifier-key behavior are covered by separate Qt mouse-event smokes below.
- The app-side smoke hook also has a green Animate/Edit Qt mouse-event
  transform check via `mise run gui-smoke-viewer-animate-tool-mouse-events-qt6`.
  It creates a visible raster scene, activates `T_Edit` on `Col1`, maps the
  same transform path through `SceneViewer` Qt mouse press/move/release events
  using the viewer DPR, and requires `mouseEventProbe=ok`,
  `toolTransformProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
  changed `TStageObject` placement, changed pixels, and visible red content
  pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, stage object X
  `0.0000 -> 1.3521`, stage object Y `0.0000 -> -0.6761`, `280162` changed
  pixels, and `357248` red pixels. This covers Qt widget event dispatch and
  high-DPI coordinate mapping for the Animate/Edit transform path;
  selection-handle hit-testing, cursor feedback, real OS-level or hardware input
  delivery, and full transform workflow visual parity remain open. Modifier-key
  behavior is covered by the separate Qt mouse-event modifier smoke below.
- The app-side smoke hook also has a green Animate/Edit Qt mouse-event undo-redo
  check via `mise run gui-smoke-viewer-animate-tool-undo-redo-qt6`. It creates a
  visible raster scene, activates `T_Edit` on `Col1`, maps the transform path
  through `SceneViewer` Qt mouse press/move/release events using the viewer DPR,
  resets the undo stack before the drag, and requires one undo entry, exact undo
  restoration, redo restoration, high-DPI framebuffer probes, changed pixels, and
  visible red content pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  stage object X `0.0000 -> 1.3521 -> 0.0000 -> 1.3521`, stage object Y
  `0.0000 -> -0.6761 -> 0.0000 -> -0.6761`, undo history `0 -> 1`, history
  indices `0/1/0/1`, `280162` changed pixels, and `357248` red pixels. This
  narrows Qt widget event dispatch, high-DPI coordinate mapping, and undo/redo
  integration for the Animate/Edit transform path; real OS-level input delivery,
  selection-handle hit-testing, full cursor artwork/hover variants, and full
  transform workflow visual parity remain open.
- The app-side smoke hook also has a green Animate/Edit Qt mouse-event modifier
  check via `mise run gui-smoke-viewer-animate-tool-modifiers-qt6`. It creates
  a visible raster scene, activates `T_Edit` on `Col1`, maps normal, Alt, and
  Shift drags through `SceneViewer` Qt mouse events using the viewer DPR, and
  requires Qt mouse-event dispatch, transform movement, Alt precision ratio,
  Shift dominant-axis lock, precise-cursor state, a nonblank framebuffer, and
  visible red content pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  normal delta `1.3521,-0.6761`, Alt delta `0.1352,-0.0676`, Shift delta
  `1.3521,0.0000`, precise cursor `false -> true`, `203603` changed pixels,
  and `357248` red pixels. This covers Qt modifier delivery, Alt precision
  transform, Shift axis constraint, and the Animate/Edit precise-cursor state;
  real OS-level input delivery, selection-handle hit-testing, full cursor
  artwork/hover variants, and full transform workflow visual parity remain open.
- The app-side smoke hook also has a green Animate/Edit handle hit-test check
  via `mise run gui-smoke-viewer-animate-tool-handles-qt6`. It creates the same
  visible raster scene, activates `T_Edit` on `Col1`, sets the active axis to
  `All`, hovers the rotation handle, then drags the rotation, scale, and center
  handles through `SceneViewer` Qt mouse events using the viewer DPR. It
  requires Qt mouse-event dispatch, handle hover feedback, handle hit-test
  movement, a nonblank framebuffer, changed hover pixels, changed final pixels,
  and visible red content pixels. The latest packaged Qt 6 and Qt 5 runs both
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  active axis `All`, handle unit `0.8585`, rotation angle
  `0.0000 -> -44.5595`, scale `1.0000 -> 1.5039`, center
  `0.0000,0.0000 -> 0.3863,0.1932`, `570` hover changed pixels, `2185` final
  changed pixels, and `357521` red pixels. This covers app-side Animate/Edit
  rotation, scale, and center handle hover and hit-test dragging through Qt
  mouse events; real OS-level input delivery, cursor artwork variants beyond
  the separate mode cursor smoke, and full manual transform workflow visual
  parity remain open.
- The app-side smoke hook also has a green Animate/Edit handle-variant check
  via `mise run gui-smoke-viewer-animate-tool-handle-variants-qt6`. It creates
  the same visible raster scene, activates `T_Edit` on `Col1`, sets the active
  axis to `All`, then drives the All-mode fallback translation path plus the
  ScaleXY and Shear handles through `SceneViewer` Qt mouse events using the
  viewer DPR. It requires Qt mouse-event dispatch, changed fallback translation
  X/Y values, changed ScaleXY X/Y values, changed Shear X/Y values, a nonblank
  framebuffer, changed pixels, and visible red content pixels. The latest
  packaged Qt 6 and Qt 5 runs both reported logical size `994x819`, DPR `2` /
  `2.00`, framebuffer `1988x1638`, fallback translation
  `0.0000,0.0000 -> 0.4829,-0.5634`, ScaleXY
  `1.0000,1.0000 -> 1.1534,1.0588`, Shear
  `0.0000,0.0000 -> -0.2146,0.1545`, `58364` changed pixels, and `368866` red
  pixels. This covers app-side Animate/Edit All-mode fallback translation,
  nonuniform ScaleXY, and Shear handle variants through Qt mouse events; real
  OS-level input delivery, full cursor artwork variants, and full manual
  transform workflow visual parity remain open.
- The app-side smoke hook also has a green Animate/Edit active-axis drag check
  via `mise run gui-smoke-viewer-animate-tool-axis-drags-qt6`. It creates the
  same visible raster scene, activates `T_Edit` on `Col1`, then separately sets
  Position, Rotation, Scale, Shear, and Center active-axis modes and drags each
  through `SceneViewer` Qt mouse events using the viewer DPR. It requires Qt
  mouse-event dispatch, changed stage-object X/Y, angle, scale, shear, and
  center values, a nonblank framebuffer, changed pixels, and visible red
  content pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, Position
  `0.0000,0.0000 -> 2.7042,-1.3521`, Rotation `0.0000 -> -44.5595`, Scale
  `1.0000 -> 1.5039`, Shear `0.0000,0.0000 -> -0.2146,-0.1545`, Center
  `0.0000,0.0000 -> 0.3863,0.1932`, `3099` changed pixels, and `358145` red
  pixels. This covers app-side Animate/Edit active-axis transform dragging for
  the five primary axis modes through Qt mouse events; real OS-level input
  delivery, full cursor artwork variants, and full manual transform workflow
  visual parity remain open.
- The app-side smoke hook also has a green Animate/Edit mode-cursor check via
  `mise run gui-smoke-viewer-animate-tool-cursors-qt6`. It creates the same
  visible raster scene, activates `T_Edit` on `Col1`, sends Qt mouse-move
  events through `SceneViewer` for Position, Rotation, Scale, Shear, and Center
  active-axis modes, and requires expected normal cursor IDs, Alt
  precise-decoration cursor IDs, cursor pixmap availability for every checked
  cursor, high-DPI framebuffer probes, nonzero changed pixels, and visible red
  content pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, Position cursor
  `23/2097175`, Rotation cursor `33/2097185`, Scale cursor `64/2097216`, Shear
  cursor `38/2097190`, Center cursor `23/2097175`, `580` changed pixels, and
  `358176` red pixels. A follow-up run also records normal/Alt cursor artwork
  signatures for the checked Position, Rotation, Scale, Shear, and Center
  cursors. The latest packaged Qt 6 and Qt 5 runs matched exactly: all cursor
  pixmaps are `32x32@1.00` with hotspot `15,15`; Position/Center hashes are
  `1e19f313f559b6d3` and `b756652c7d59b6d3`, Rotation hashes are
  `137572f5ab7d9ccd` and `e5f471814b7d9ccd`, Scale hashes are
  `85c794af41f0f0e3` and `84d4229245f0f0e3`, and Shear hashes are
  `8161bce21e155637` and `812dd0e66155637`. This covers app-side Animate/Edit
  mode-specific cursor IDs, Alt precise-cursor decoration, cursor resource
  availability, and cursor artwork signatures through Qt mouse-move delivery;
  real OS-level cursor delivery, unchecked cursor artwork variants, and full
  manual transform workflow visual parity remain open.
- The app-side smoke hook also has a green Selection tool vector-handle check
  via `mise run gui-smoke-viewer-selection-tool-vector-handles-qt6`. It creates
  a vector stroke, switches the current tool to `T_Selection`, uses rectangular
  selection through `SceneViewer` Qt mouse events, hovers the selected vector
  stroke bbox scale handle, then drags that handle through Qt mouse events using
  the viewer DPR. It requires a valid Selection tool state, rectangular
  selection mode/type, one selected stroke, scale-cursor artwork, a changed
  selected-stroke bbox, a nonblank framebuffer, changed pixels, and visible red
  content pixels. The latest packaged Qt 6 and Qt 5 runs both reported logical
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, selected stroke
  count `1`, bbox
  `-208.0000,-138.0000,208.0000,33.0000 ->
  -209.6763,-136.2378,279.8100,81.0663`, bbox delta `73.4863,46.3041`, cursor
  ID `38`, cursor artwork `ok`, `195080` changed pixels, and `174082` red
  pixels. This covers app-side vector Selection tool rectangular selection,
  scale-handle hover feedback, cursor resource availability, and scale-handle
  dragging through Qt mouse events. Separate smokes now cover advanced vector
  handles and vector Freehand/Polyline mode variants; real OS-level
  input/cursor delivery, raster selection workflows beyond
  rectangular/freehand/polyline app-side coverage, multi-object selection
  workflows, and full manual selection visual parity remain open.
- The app-side smoke hook also has a green Selection tool vector handle-variant
  check via
  `mise run gui-smoke-viewer-selection-tool-vector-handle-variants-qt6`. It
  creates the same vector stroke, switches the current tool to `T_Selection`,
  uses rectangular selection through `SceneViewer` Qt mouse events, then hovers
  and drags the horizontal edge-scale handle, vertical edge-scale handle, and
  top-right rotation handle. It requires a valid Selection tool state,
  rectangular selection mode/type, one selected stroke, cursor artwork for each
  checked handle, changed vector bbox measurements, a nonblank framebuffer,
  changed pixels, and visible red content pixels. The latest packaged Qt 6 and
  Qt 5 runs both reported logical size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, selected stroke count `1`, horizontal-scale bbox
  `-208.0000,-138.0000,208.0000,33.0000 ->
  -206.1030,-139.7834,260.7839,34.7834`, width delta `50.8869`, horizontal
  cursor `40`, vertical-scale bbox
  `-206.1030,-139.7834,260.7839,34.7834 ->
  -209.6923,-135.7615,264.3733,75.3727`, height delta `36.5673`, vertical
  cursor `41`, rotation bbox
  `-209.6923,-135.7615,264.3733,75.3727 ->
  -188.4725,-179.3471,275.3424,77.2171`, rotation cursor `33`, `198156`
  changed pixels, and `163929` red pixels. This covers app-side vector
  Selection tool rectangular selection, horizontal/vertical edge-scale handle
  cursor feedback, rotation handle cursor feedback, cursor resource
  availability, and handle dragging through Qt mouse events; real OS-level
  input/cursor delivery, raster selection workflows beyond
  rectangular/freehand/polyline app-side coverage, multi-object selection
  workflows, and full manual selection visual parity remain open.
- The app-side smoke hook also has a green Selection tool vector
  center/thickness/free-deform check via
  `mise run gui-smoke-viewer-selection-tool-vector-center-thickness-deform-qt6`.
  It creates the same vector stroke, switches the current tool to
  `T_Selection`, uses rectangular selection through `SceneViewer` Qt mouse
  events, then hovers and drags the center handle, global thickness handle, and
  Ctrl free-deform top-right handle. It requires a valid Selection tool state,
  rectangular selection mode/type, one selected stroke, cursor artwork for each
  checked handle, moved center coordinates, changed average stroke thickness,
  changed free-deform bbox measurements, a nonblank framebuffer, changed
  pixels, and visible red content pixels. The latest packaged Qt 6 and Qt 5
  runs both reported logical size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, selected stroke count `1`, center
  `0.0000,-52.5000 -> 44.6412,-86.8394`, center cursor `31`, average
  thickness `22.6667 -> 31.5949`, thickness cursor `32`, free-deform bbox
  `-216.9282,-146.9282,216.9282,41.9282 ->
  -216.1371,-152.5195,224.3624,29.0765`, free-deform cursor `20`, `66299`
  changed pixels, and `166236` red pixels. This covers app-side vector
  Selection tool center-handle, global-thickness, and Ctrl free-deform cursor
  feedback plus handle dragging through Qt mouse events; real OS-level
  input/cursor delivery, multi-object/multi-frame selection workflows, and full
  manual selection visual parity remain open.
- The app-side smoke hook also has a green Selection tool vector mode check via
  `mise run gui-smoke-viewer-selection-tool-vector-mode-variants-qt6`. It
  creates two separated vector strokes, switches the current tool to
  `T_Selection`, sets the vector Selection type to `Freehand`, lasso-selects
  the left stroke through `SceneViewer` Qt mouse events, clears the selection,
  then sets the type to `Polyline` and polygon-selects the right stroke through
  the same viewer event path. It requires a valid Selection tool state,
  Freehand and Polyline vector selection types, two vector strokes, one selected
  stroke after each mode, stroke `0` selected only by Freehand, stroke `1`
  selected only by Polyline, distinct selected bboxes, a nonblank framebuffer,
  changed pixels, and visible red content pixels. The latest packaged Qt 6 run
  reported logical size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  selection mode `Standard`, freehand type `Freehand`, polyline type
  `Polyline`, vector stroke count `2`, freehand count `1`, freehand stroke
  booleans `true/false`, freehand bbox
  `-254.0000,-119.0000,-51.0000,29.0000`, polyline count `1`, polyline stroke
  booleans `false/true`, polyline bbox `41.0000,-109.0000,269.0000,39.0000`,
  bbox changed from freehand `true`, `4249` changed pixels, and `132404` red
  pixels. The latest packaged Qt 5 run matched those state and bbox
  measurements with `3989` changed pixels and `104339` red pixels. This covers
  app-side vector Selection tool Freehand and Polyline selection through Qt
  mouse events; real OS-level input/cursor delivery, multi-object/multi-frame
  selection workflows, and full manual selection visual parity remain open.
- The app-side smoke hook also has a green Selection tool raster-handle check
  via `mise run gui-smoke-viewer-selection-tool-raster-handles-qt6`. It creates
  a full-color raster level, switches the current tool to `T_Selection`, uses
  rectangular selection through `SceneViewer` Qt mouse events, hovers the
  selected raster bbox scale handle, then drags that handle through Qt mouse
  events using the viewer DPR. It requires a valid Selection tool state,
  rectangular raster selection type, a non-empty raster selection,
  scale-cursor artwork, floating selection state after the drag, a changed
  raster selection bbox, a nonblank framebuffer, changed pixels, and visible
  red content pixels. The latest packaged Qt 6 run reported logical size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, empty state
  `true -> false`, floating state `false -> true`, bbox
  `-90.0000,-85.0000,95.0000,90.0000 ->
  -90.0000,-85.0000,150.2195,135.1961`, bbox delta `55.2195,45.1961`, cursor
  ID `38`, cursor artwork `ok`, `39449` changed pixels, and `391090` red
  pixels. The latest packaged Qt 5 run matched those measurements except for
  `39448` changed pixels. This covers app-side raster Selection tool
  rectangular selection, scale-handle hover feedback, cursor resource
  availability, and scale-handle dragging through Qt mouse events; real
  OS-level input/cursor delivery, raster selection workflows beyond
  rectangular/freehand/polyline app-side coverage, multi-object and multi-frame
  selection workflows, and full manual selection visual parity remain open.
- The app-side smoke hook also has a green Selection tool raster mode check via
  `mise run gui-smoke-viewer-selection-tool-raster-mode-variants-qt6`. It
  creates a full-color raster level, switches the current tool to
  `T_Selection`, sets the raster Selection type to `Freehand`, replays a lasso
  path through `SceneViewer` Qt mouse events, then sets the type to `Polyline`
  and replays a click/double-click polygon through the same viewer event path.
  It requires a valid Selection tool state, Freehand and Polyline raster
  selection types, non-empty non-floating raster selections, distinct selected
  bboxes, a nonblank framebuffer, changed pixels, and visible red content
  pixels. The latest packaged Qt 6 run reported logical size `994x819`, DPR
  `2` / `2.00`, framebuffer `1988x1638`, selection mode `not-applicable`,
  freehand type `Freehand`, polyline type `Polyline`, freehand empty state
  `true -> false`, freehand floating state `false`, freehand bbox
  `-128.0000,-128.0000,-2.6106,74.1808`, polyline empty state `false`,
  polyline floating state `false`, polyline bbox
  `35.1826,-94.8777,128.0000,105.1491`, bbox changed from freehand `true`,
  `2974` changed pixels, and `356418` red pixels. The latest packaged Qt 5 run
  matched those state and bbox measurements with `2781` changed pixels and
  `356418` red pixels. This covers app-side raster Selection tool Freehand and
  Polyline selection through Qt mouse events; real OS-level input/cursor
  delivery, raster selection workflows beyond rectangular/freehand/polyline
  app-side coverage, multi-object and multi-frame selection workflows, and full
  manual selection visual parity remain open.
- The app-side smoke hook also has green vector and full-color raster brush
  tool-input checks via `mise run gui-smoke-viewer-vector-brush-qt6` and
  `mise run gui-smoke-viewer-raster-brush-qt6`. It creates a sandbox level,
  selects `T_Brush`, replays a direct left-button stroke path through the tool,
  requires the vector stroke count or raster opaque/red pixel counts to
  increase, and verifies changed plus red-dominant `SceneViewer` framebuffer
  pixels. These are the first drawing tool guards in the Qt 6 lane, but
  OS-level event delivery, high-DPI input mapping, timeline/UI onion workflow,
  overlays, manual FX Preview UI and broader FX preview workflows, broader
  final-render parity, and Qt 5-vs-Qt 6 visual parity remain open.
- The app-side smoke hook also has a green raster brush event-dispatch check via
  `mise run gui-smoke-viewer-raster-brush-mouse-events-qt6`. It sends Qt mouse
  press/move/release events through `SceneViewer`, maps the shared world-space
  brush path into logical widget coordinates using the viewer DPR, verifies
  increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. This narrows Qt widget event
  dispatch and high-DPI input coordinate coverage; real OS-level event delivery,
  tablet input, timeline/UI onion workflow, overlays, manual FX Preview UI and
  broader FX preview workflows, broader final-render parity, and visual parity
  against the Qt 5 baseline remain open.
- The app-side smoke hook also has a green raster brush synthetic tablet-event
  check via `mise run gui-smoke-viewer-raster-brush-tablet-events-qt6`. It
  sends Qt tablet press/move/release events through `SceneViewer`, maps the
  shared world-space brush path into logical widget coordinates using the
  viewer DPR, drives pressure from 0.35 to 0.85 with tilt `-18,12`, verifies
  increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. This narrows Qt tablet-event
  dispatch and `TMouseEvent` tablet/pressure propagation. Real tablet hardware
  is not available for this migration pass, so hardware pressure/tilt should
  stay deferred rather than block the Qt 6 port unless a device or credible
  OS-level simulator is added;
  OS-level event delivery, platform driver pressure/tilt, timeline/UI onion
  workflow, overlays, manual FX Preview UI and broader FX preview workflows,
  broader final-render parity, and visual parity against the Qt 5 baseline
  remain open.
- The app-side smoke hook also has a green non-rendering high-DPI diagnostic
  check via `mise run gui-smoke-highdpi-qt6`. It records main-window DPR,
  screen DPR, logical DPI, screen geometry, and Qt 6 high-DPI mode from the
  packaged app. This narrows startup/window diagnostics, but it does not prove
  viewer scaling, canvas pixels, overlay positioning, preview render, or final
  render correctness.
- The app-side smoke hook also has a non-rendering Qt Multimedia device
  enumeration check via `mise run gui-smoke-media-devices-qt6`. It records the
  active Qt Multimedia API lane plus audio input, audio output, and camera
  counts/default names from the packaged app. This narrows the audio/camera
  diagnostic path, but it does not prove audio playback, audio recording,
  camera permission prompts, preview frames, or still capture correctness.
- The app-side smoke hook also has a Qt 6 default camera-format enumeration
  check via `mise run gui-smoke-camera-formats-qt6`. It records default camera
  format count, resolution bounds, frame-rate bounds, and compact pixel-format
  metadata without starting preview or capture. The current local packaged run
  found one video input and seven default-camera formats. This narrows packaged
  camera metadata diagnostics, but it does not prove permission prompts, preview
  frames, still capture, hotplug behavior, or stop-motion UI workflows.
- The app-side smoke hook also has a Qt 6 audio-output backend check via
  `mise run gui-smoke-audio-output-qt6`. It starts the packaged app's default
  audio output with a short silent buffer and verifies `QAudioSink` reports no
  error. This narrows the playback-backend path, but it does not validate
  audible output quality, lip-sync preview timing, recording, or microphone
  permission behavior.
- The app-side smoke hook also has a Qt 6 audio-input backend check via
  `mise run gui-smoke-audio-input-qt6`. It starts the packaged app's default
  audio input with `QAudioSource`, captures a short buffer when the session can
  deliver microphone data, and verifies `NoError`. The smoke now has a bounded
  app-side timeout so microphone permission/device stalls report structured
  status instead of hanging. The current local aggregate run reported
  `audioInputProbe=timeout` and was treated as an environment skip. This keeps
  the backend smoke useful in headless or permission-limited sessions, but it
  does not validate microphone recording parity, the Record Audio popup, WAV
  save/reload behavior, permission UX, lip-sync timing, or capture quality.
- The app-side smoke hook also has a Qt 6 audio-recording WAV check via
  `mise run gui-smoke-audio-recording-wav-qt6`. It starts the packaged app's
  default audio input, records through `AudioWriterWAV`, saves a short WAV, and
  reloads it through `TSoundTrackReader` when microphone capture is available.
  This smoke also has a bounded app-side timeout. The current local aggregate
  run reported `audioRecordingWavProbe=timeout` and was treated as an
  environment skip. This narrows harness behavior and prevents hung GUI smoke
  runs, but it does not validate the Record Audio writer path on this session,
  the Record Audio popup button flow, Save and Insert column insertion,
  microphone permission UX, lip-sync timing, or capture quality.
- The app-side smoke hook also has a Qt 6 generated-WAV playback check via
  `mise run gui-smoke-audio-playback-wav-qt6`. It writes a low-volume PCM16 WAV
  through `AudioWriterWAV`, reloads it through `TSoundTrackReader`, opens the
  default output through `TSoundOutputDevice`, and verifies playback starts and
  stops. The current local packaged run used MacBook Pro Speakers at 44.1 kHz
  mono, wrote 264,600 bytes to a 264,644-byte WAV, and reloaded 132,300 samples
  over 3,000 ms. This narrows the product output-start path, but it does not
  validate audible output quality, chosen speaker route, flipbook/timeline
  playback, lip-sync preview timing, or Record Audio UI behavior.
- The first scene-creation attempt exposed a crash in
  `TThread::ExecutorImp::refreshAssignments()` during `IconGenerator`
  invalidation after scene save. The shared task scheduler now avoids
  erase-then-decrement iterator invalidation in both
  `ExecutorImp::refreshAssignments()` and `Worker::takeTask()`. Both Qt 6 and
  Qt 5 `OpenToonz` app targets rebuild after this shared fix.
- This is still not broad rendering validation. Narrow raster-cell,
  vector-cell, direct viewer zoom/pan, app-side onion-skin, row-area onion
  marker UI, mobile drag ranges, fixed marker add/remove/front drags,
  onion-skin context-menu commands, custom onion colors, row-area onion marker
  behavior across both current xsheet/timeline orientations, camera-box, and
  safe-area/field-guide, safe-area
  presets/colors, safe-area custom files, field-guide settings, ruler/guide,
  ruler-guide event,
  ruler-guide variant, ruler guide-line rendering, ruler tick rendering,
  ruler/guide style, Animate/Edit tool overlay,
  direct Animate/Edit tool transform-drag, Qt mouse-event Animate/Edit
  transform, and Animate/Edit
  undo-redo/modifier-key/handle hit-test/handle-variant/active-axis/mode-cursor
  `SceneViewer` framebuffer smokes plus cursor artwork signatures, and
  Selection tool vector rectangular selection, scale-handle dragging, and
  edge-scale/rotation/center/thickness/free-deform handle variants plus vector
  Freehand/Polyline mode variants, raster Selection rectangular selection,
  scale-handle dragging, Freehand mode, and Polyline mode are green,
  but drawing workflows, remaining timeline onion workflows beyond the current
  app-side row-area marker/toggle/drag, fixed-marker, Shift and Trace,
  context/color/orientation coverage, remaining selection workflows beyond
  vector rectangular/freehand/polyline mode coverage, vector
  scale/edge/rotation/center/thickness/free-deform handle paths, and raster
  rectangular/freehand/polyline app-side paths, remaining
  cursor artwork outside the checked Animate/Edit mode cursor set,
  real OS-level transform dragging, remaining manual ruler-guide variants not
  covered by app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, broader vector rendering, broader
  high-DPI visual behavior, timeline/xsheet interaction, manual Preview/FX
  Preview UI and broader production preview scenes, broader final-render
  parity, and Qt 5-vs-Qt 6 visual parity remain open and are active Qt 6
  parity work.

### 6. Platform Packaging

Packaging has to be treated as part of the port.

macOS:

- Add a Qt 6 package lane rather than mutating the Qt 5 package script in place.
- Re-audit plugin group names and deployed library names after `macdeployqt`.
- Re-check app bundle structure, ad hoc signing, release signing, notarization,
  helper executables, and copied `stuff`.
- Verify whether Qt 6 rendering, multimedia, or helper-process changes require
  bundled framework or entitlement updates.

Current branch status:

- `OPENTOONZ_QT_PLUGIN_DIRS` now includes the QtSvg plugin root in addition to
  QtBase and QtMultimedia. This is needed for SVG image/icon plugins during
  build-tree and packaged runs.
- The macOS Nix packaging script now recognizes Qt 6's `multimedia` plugin
  group and `iconengines`, while preserving the existing Qt 5 plugin groups.
- The packaging script now rewrites copied Qt plugin framework references into
  `Contents/Frameworks`; without this, the packaged Qt 6 app could find the
  `cocoa` plugin but abort because the plugin loaded a second Qt copy from the
  Nix store.
- The packaging script also rewrites copied framework and library install IDs,
  so bundled Qt plugin dependencies identify themselves through
  `@executable_path/../Frameworks` instead of a Nix-store path.
- `mise` now has `package-macos-qt6`, `check-macos-arm64-qt6`,
  `gui-smoke-qt6`, `gui-smoke-scene-create-qt6`,
  `gui-smoke-highdpi-qt6`, and `gui-smoke-xsheet-scrub-qt6` tasks for the
  packaged Qt 6 app bundle.
- `mise run package-macos-qt6` now completes and ad hoc signs the Qt 6 bundle.
  On 2026-05-31, `mise run check-macos-arm64-qt6` reports the main binary as
  arm64 and checks 291 Mach-O files for arm64.
- The first Qt 6 Cocoa startup smoke reported missing multimedia backends and
  SVG pixmap load failures. After the plugin-path fix, a second bounded Cocoa
  smoke from the Qt 6 Nix shell loaded the Qt Multimedia FFmpeg backend and no
  longer reported SVG pixmap failures. That earlier bounded harness stopped the
  app after startup and therefore could not validate user workflows.
- A repeatable bounded startup smoke of the packaged Qt 6 app now gets past the
  prior duplicate-Qt/platform-plugin abort, loads the Qt Multimedia FFmpeg
  backend from the package, and stays running long enough for startup
  inspection. After the latest relink and packaging pass, the same bounded
  startup smoke still passes. The smoke still reports OpenGL fallback messages,
  so it is not a substitute for viewer workflow validation.
- The GUI smoke harness accepts `OPENTOONZ_GUI_SMOKE_FILE` for scene-open
  diagnostics and now uses `OPENTOONZ_GUI_SMOKE_STATUS_FILE` for app-side
  create/open verification. The System Events path remains a fallback, but the
  green Qt 6 create/open evidence comes from the app-side status file and the
  generated scene artifact.
- Bounded GUI and script smokes now use SIGKILL for intentional harness cleanup.
  This avoids misleading crash-handler `atos` output caused by OpenToonz
  treating SIGTERM as a crash signal.

Windows:

- The current workflow depends on a custom Qt 5.15.2 with WinTab support.
  Existing repo docs say this custom build carries a WinTab feature that was
  introduced officially in Qt 6. A Qt 6 Windows lane should verify whether the
  custom Qt package can be removed.
- Replace the manual Qt 5 path setup with a reproducible Qt 6 acquisition
  strategy.
- Re-run tablet, stylus, OpenGL rendering, camera, and audio checks.
- Re-audit `windeployqt` arguments and plugin output.

Linux:

- Add an Ubuntu Qt 6 CI lane before replacing the Qt 5 lane.
- Audit package names for Qt 6 equivalents.
- Re-test Linux deploy tooling with Qt 6 plugin discovery and runtime library
  paths.
- Check Wayland and X11 behavior separately if the Qt 6 runtime includes both.

### 7. Translations, Resources, And UI Data

The project uses Qt resources, MOC, and Qt Linguist translation files.

Expected changes:

- Convert `qt5_add_resources` calls to a wrapper that calls the right Qt major
  command.
- Convert `qt5_wrap_cpp` calls to a wrapper. Consider moving toward CMake
  `AUTOMOC` later, but do not combine that with the first Qt 6 compile bring-up.
- Convert `qt5_create_translation` in the translation helper to a version-aware
  command.
- Keep `WITH_TRANSLATION=OFF` for early Qt 6 CI until the app compiles.
- Add a later translation-specific milestone that regenerates `.qm` files only
  when intentionally updating packaged localization.

## Proposed Migration Plan

### Phase 0: Freeze The Baseline And Qt 6 Scope

Goal: preserve the Qt 5 baseline while making Qt 6 the active migration focus.

Tasks:

- Confirm the Qt 5 default build still configures with `mise run configure`.
- Record that Metal is parked until the Qt 6 port is complete.
- Use the current Qt 5 application behavior as the visual-parity baseline for
  Qt 6 rendering work.
- Create a Qt 6 tracking document or issue with ownership by subsystem.
- Capture the current Qt 5 module list and all Qt 5-specific CMake references.

Exit criteria:

- Qt 5 default path remains unchanged.
- There is a written list of Qt 6 rendering, viewer, multimedia, scripting, and
  packaging areas that still need product-parity validation.

### Phase 1: Qt 5.15-Compatible Cleanup

Goal: reduce known Qt 6 compile failures without changing the Qt major.

Tasks:

- Add a diagnostic `QT_DISABLE_DEPRECATED_UP_TO=0x050F00` lane.
- Run Clazy Qt 6 checks against the Qt 5 build.
- Port `QDesktopWidget` and `qApp->desktop()` to a local screen helper.
- Keep `QRegExp` and `QRegExpValidator` removed with
  `mise run check-no-qregexp`.
- Keep direct `QGLWidget::convertToGLFormat` calls out of feature code; the
  current tool call sites route through `QtCompat::convertToGLFormat`.
- Keep direct `QCheckBox::stateChanged` connects out of feature code with
  `mise run check-qt6-checkbox-state-scope`; use
  `QtCompat::connectCheckStateChanged()` for checkbox check-state changes.
- Extend the `ttextcodec.h` adapter only where legacy encodings still require
  exact behavior, and keep direct `QTextCodec` usage out of feature code.

Exit criteria:

- Qt 5 default build still works.
- The diagnostic deprecated-API lane has fewer failures and a documented
  remaining list.

### Phase 2: Dual Qt CMake Lane

Goal: make Qt 6 configuration first-class without breaking Qt 5.

Tasks:

- Add `OPENTOONZ_QT_MAJOR`.
- Add Qt wrapper functions for MOC, resources, and translations.
- Convert CMake links from hard-coded `Qt5::` to version-aware targets.
- Add Qt 6 Nix inputs next to Qt 5 inputs.
- Add Qt 6 CMake presets and mise tasks.
- Add temporary `Core5Compat` only where needed.
  The current branch has already narrowed this to `image`, `toonzlib`, and
  `OpenToonz` through `${QT_CORE5COMPAT_LINK_TARGETS}`.
- Add `OpenGLWidgets` for `QOpenGLWidget` users.
- Add `Qml` for the future script runtime.

Exit criteria:

- Qt 5 `mise run configure` and `mise run build` still work.
- Qt 6 `mise run configure-qt6` reaches source compilation or produces a
  truthful, documented missing-module error.

### Phase 3: Script Runtime Port

Goal: remove the `Qt5::Script` dependency.

Tasks:

- Introduce a scripting facade independent of `QScriptEngine`.
- Move current Qt 5 script engine behind the facade.
- Implement the Qt 6 `QJSEngine` backend.
- Port script bindings one group at a time:
  - files and paths
  - scenes and levels
  - images and rasterizer
  - renderer
  - vectorizers and converters
- Add script fixtures that run in both backends until the Qt 5 backend is
  retired.

Exit criteria:

- No production target links `Qt5::Script` in the Qt 6 lane.
- A set of script compatibility fixtures passes under the Qt 6 backend.

### Phase 4: Multimedia And Hardware

Goal: move camera, audio, and media playback to Qt 6 APIs.

Tasks:

- Port audio playback in `common/tsound/tsound_qt.cpp`.
- Port audio recording popup behavior.
- Port auto lip-sync preview playback.
- Port stop-motion camera enumeration, preview, still capture, and frame
  ingestion.
- Keep `QAbstractVideoSurface` out of every Qt 6 production source list. If the
  legacy 32-bit `penciltestpopup_qt.*` fallback becomes part of the Qt 6
  release scope, replace that fallback with `QVideoSink`-based frame handling.
- Add manual smoke scripts or repo-local checklists for camera and audio.

Exit criteria:

- App compiles with Qt 6 multimedia.
- Camera and audio smoke checks are documented and at least one local platform
  has been verified.

### Phase 5: Qt 6 Rendering And Viewer Integration

Goal: make Qt 6 match current OpenToonz viewer, drawing, and rendering
behavior.

Tasks:

- Update viewer widgets for Qt 6 event, high-DPI, and OpenGLWidgets behavior.
- Keep direct GL compatibility only where required by non-macOS paths.
- Validate frame rendering, viewer drawing, onion skin, selection overlays,
  tool cursors, manual FX Preview UI, manual preview-save
  popup/overwrite/warning dialogs, live flipbook playback behavior, and broader
  production FX Preview scenes.

Exit criteria:

- Qt 6 build runs the app on at least one desktop platform.
- A GUI smoke pass exercises viewer and drawing behavior.
- Known Qt 6-vs-Qt 5 visual parity differences are documented.

### Phase 6: Packaging And CI Expansion

Goal: make Qt 6 installable and testable by maintainers.

Tasks:

- Add macOS Qt 6 package task.
- Add Linux Qt 6 CI lane.
- Add Windows Qt 6 CI lane.
- Re-audit deployment tools, plugin copies, helper executables, and `stuff`
  layout.
- Preserve Qt 5 CI until Qt 6 is release-ready.

Exit criteria:

- Qt 6 artifacts exist for the target platforms.
- Packaging does not depend on accidental local Qt paths.
- Runtime smoke checks pass from packaged artifacts.

### Phase 7: Remove Compatibility Bridges

Goal: make the port maintainable instead of permanently transitional.

Tasks:

- Remove remaining `Core5Compat` uses unless there is a documented legacy
  encoding requirement still owned by `ttextcodec.h`.
- Remove Qt 5-only CMake shims if maintainers decide to drop Qt 5.
- Remove or archive Qt 5 CI after release sign-off.
- Update platform build docs.

Exit criteria:

- Qt 6 is the default build.
- Qt 5-specific dependencies are gone from Nix, CI, and documentation or are
  explicitly retained for a supported maintenance branch.

## Estimate And Risk

This is likely a multi-month migration for a small team or a sequence of many
agent-assisted implementation sessions.

Approximate effort by area:

| Area | Relative effort | Risk |
|---|---:|---|
| CMake and Nix dual lane | Medium | Medium |
| `QDesktopWidget`, removed regex APIs, small deprecations | Medium | Low to medium |
| `QTextCodec` and legacy encodings | Medium | Medium |
| Qt Script replacement | Large | High |
| Qt Multimedia camera/audio | Large | High |
| OpenGL/Qt 6 viewer integration | Large | High |
| macOS packaging | Medium | Medium |
| Windows packaging and tablet validation | Large | High |
| Linux packaging | Medium | Medium |
| QA and release stabilization | Large | High |

The most important schedule decision has changed: Qt 6 is now the active
priority, and Metal should be revisited only after the Qt 6 port reaches
product parity. Qt 6 rendering work should proceed directly against the current
OpenToonz behavior.

## Recommended Next Implementation Slice

The first infrastructure slice has now been achieved in this branch: the dual
Qt lane exists and both Qt 5 and Qt 6 can compile/link the `OpenToonz` app
target. The next implementation branch should not repeat that runway work. It
should turn the compile/link milestone into a runtime and subsystem validation
milestone:

1. Continue packaged Qt 6 interactive GUI validation beyond the now-green
   scripted startup/create/open/xsheet-state, raster/vector viewer-framebuffer,
   direct viewer zoom/pan transform, app-side onion-skin, row-area onion marker
   UI, mobile drag ranges, fixed marker add/remove/front drags, onion-skin
   context-menu commands, custom onion colors, row-area onion marker
   orientation coverage, camera-box,
   safe-area/field-guide, safe-area presets/colors, safe-area custom files,
   field-guide settings,
   ruler/guide, ruler-guide event, ruler-guide variant, ruler guide-line
   rendering, ruler tick rendering, ruler/guide style,
   Animate/Edit tool overlay framebuffer, direct Animate/Edit tool
   transform-drag, Qt mouse-event Animate/Edit transform-drag plus undo-redo,
   modifier-key behavior, app-side rotation/scale/center handle hit-testing,
   app-side All-mode fallback translation/ScaleXY/Shear handle variants,
   Position/Rotation/Scale/Shear/Center active-axis transform dragging, mode
   cursor feedback, Selection tool vector rectangular selection, scale-handle
   dragging, and edge-scale/rotation/center/thickness/free-deform handle
   variants, vector Selection Freehand and Polyline mode variants, raster
   Selection rectangular selection and scale-handle dragging, raster Selection
   Freehand and Polyline mode variants,
   Previewer render-cache, PreviewFxManager flipbook/cache render-output,
   PreviewFxManager sub-camera cache render-output, and PreviewFxManager
   flipbook frame-switch/clone/freeze/unfreeze controls, plus full-frame and
   sub-camera saved-frame export,
   vector/raster brush tool-input paths, Qt mouse-event raster brush path, and
   synthetic Qt tablet-event raster brush path. A real macOS
   CGEvent/System Events raster-brush gate now exists, but the current local run
   is blocked by app activation or OS input delivery: CGEvent post access and
   AX trust are present, and the Qt global stroke points are inside the target
   OpenToonz window, but the active desktop session still leaves `loginwindow`
   frontmost, the app-side event filter sees zero mouse events, System Events
   still reports `-25200`, and raster pixels remain unchanged. Resolve or rerun
   that permissioned path in an unlocked/frontmost user session before treating
   OS-level mouse delivery as covered. Keep hardware tablet pressure/tilt
   deferred unless a real device or credible OS-level simulator becomes
   available, then continue into remaining timeline onion workflows beyond the
   current app-side row-area marker/toggle/drag, fixed-marker, Shift and Trace,
   context/color/orientation smokes, remaining selection workflows beyond
   vector rectangular/freehand/polyline mode coverage, vector
   scale/edge/rotation/center/thickness/free-deform handle paths, and raster
   rectangular/freehand/polyline app-side paths, real OS-level cursor delivery,
   manual Preview/FX Preview UI, manual preview-save popup/overwrite/warning
   dialogs, production FX-heavy preview scenes,
   remaining
   cursor artwork outside the checked
   Animate/Edit mode cursor set,
   real OS-level transform dragging, remaining manual ruler-guide
   variants not covered by app-side
   create/move/delete/drag-hide, guide-line visual variants beyond app-side
   custom-position dashed-line coverage, broader high-DPI input and
   viewer/rendering behavior, and the first remaining workflow blockers.
2. Keep the app-side GUI smoke status hook narrow and test-only. System Events
   remains useful as a fallback/manual diagnostic, but do not make Qt 6
   create/open validation depend on macOS accessibility window discovery unless
   that path becomes repeatable again.
3. Verify that app resources, `stuff`, plugins, helper executables, and runtime
   paths are sufficient outside the linker step on Linux and Windows too.
4. Extend the current script compatibility fixture set and use it to drive the
   next `QJSEngine` object-binding slice after the bootstrap `run()` error,
   narrow `FilePath`,
   FilePath-edge, FilePath metadata, path-argument, `Scene`, Scene edge-case,
   `Level`, `Level.path`,
   Level edge-case, level-wide transformer, Level I/O types including
   ToonzRaster reload frame access, scene load-level sequence,
   scene data save/reopen, Scene reload edge, Scene failed-load wrapper
   preservation,
   Scene save-icon, Scene save-icon variants,
   Scene frame-id
   handling, Scene cell frame-id type, Scene lifecycle edge cases, `Image`,
   Image edge-case, Image level-first-frame,
   `ImageBuilder`, ImageBuilder edge-case, Transform edge-case,
   ToonzRasterConverter, ToonzRasterConverter level conversion,
   ToonzRasterConverter edge-case,
   OutlineVectorizer/CenterlineVectorizer/Rasterizer including full-color
   Rasterizer output, Renderer
   renderFrame/renderScene/dumpCache, Renderer frame/column selection,
   Renderer vector input, Renderer edge-case, Wrapper id facades, and binding
   lifecycle/property edges.
   Good next candidates are remaining advanced scene APIs, broader
   scene-icon/offscreen visual parity, or another script binding subset.
5. Run focused product-level audio playback/recording and camera smoke tests on
   real hardware. The current audio-input, audio-output, audio-recording WAV,
   audio-playback WAV, and camera-format smokes are backend, WAV-writer, sound
   I/O reload, product-output-start, and enumeration guards; still add or run
   Record Audio UI Save and Insert column insertion, audible playback
   confirmation, lip-sync preview/timing, camera preview, and still-capture
   checks before calling multimedia product-ready.
6. Harden the existing macOS Qt 6 packaging lane where runtime evidence already
   requires it: package after every GUI relink, reduce repeated `macdeployqt`
   install-name churn, and then move package follow-up to Linux and Windows.
7. Bring viewer/rendering changes forward in narrow, validated slices until Qt
   6 reaches visual parity.

That slice gives later agents runtime evidence instead of another compile-only
milestone while keeping the Qt 6 port focused on product parity.

## Validation Matrix

During migration, every completed slice should state exactly which checks were
run.

Minimum checks for non-rendering Qt 6 prep:

```sh
mise run configure
mise run build
mise run check
mise run doctor-qt6
mise run configure-qt6
git diff --check
```

Additional checks once Qt 6 compilation starts:

```sh
mise run build-qt6
cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
```

Current verified compile/link checks in this branch:

```sh
cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz --parallel
cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz --parallel
```

These commands prove the app target links in both lanes. They do not replace
launch, packaging, GUI, scripting, audio, camera, or renderer smoke validation.
The latest local Qt 6 build evidence is `mise run build-qt6` on June 5, 2026.
It passed with the expanded local preflight chain enabled: Windows MSVC ABI,
QRegExp, Core5Compat scope, multimedia scope, script scope, font metrics scope,
and high-DPI startup attribute scope. The remaining compiler warnings in that
run were macOS OpenGL deprecation warnings from `tgl.h`.
After the combo-box activation and checkbox state-change helper slices,
targeted app-target rebuilds also passed on June 5, 2026 in both lanes:
`cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
--parallel` and
`cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz --parallel`.
The Qt 5 run became a broad rebuild after Ninja recovered its log, but it still
linked successfully.

Packaging checks once artifacts exist:

```sh
mise run package-macos-qt6
mise run check-macos-arm64-qt6
mise run check-no-qregexp
mise run check-core5compat-scope
mise run check-qt6-fontmetrics-scope
mise run check-qt6-highdpi-attribute-scope
mise run check-qt6-combobox-activated-scope
mise run check-qt6-checkbox-state-scope
mise run gui-smokes-app-qt6
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
mise run gui-smoke-viewer-zoom-pan-qt6
mise run gui-smoke-viewer-onion-skin-qt6
mise run gui-smoke-viewer-onion-skin-rowarea-qt6
mise run gui-smoke-viewer-onion-skin-rowarea-drag-qt6
mise run gui-smoke-viewer-onion-skin-fixed-marker-drag-qt6
mise run gui-smoke-viewer-onion-skin-context-menu-qt6
mise run gui-smoke-viewer-onion-skin-custom-colors-qt6
mise run gui-smoke-viewer-onion-skin-orientations-qt6
mise run gui-smoke-viewer-onion-skin-shift-trace-qt6
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
mise run gui-smoke-viewer-raster-brush-system-events-qt6
OPENTOONZ_GUI_SMOKE_FILE=<path-to-scene.tnz> bash scripts/qt6/run-gui-smoke.sh
mise run script-smoke-qt6
mise run script-smoke-run-errors-qt6
mise run script-smoke-filepath-qt6
mise run script-smoke-filepath-coercion-qt6
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
mise run script-smoke-scene-loadlevel-sequence-qt6
mise run script-smoke-scene-save-reopen-qt6
mise run script-smoke-scene-reload-edges-qt6
mise run script-smoke-scene-load-failure-qt6
mise run script-smoke-scene-save-icon-qt6
mise run script-smoke-scene-save-icon-variants-qt6
mise run script-smoke-scene-frameids-qt6
mise run script-smoke-level-qt6
mise run script-smoke-level-edges-qt6
mise run script-smoke-level-io-qt6
mise run script-smoke-level-io-types-qt6
mise run script-smoke-level-path-qt6
mise run script-smoke-level-path-edges-qt6
mise run script-smoke-level-lifecycle-edges-qt6
mise run script-smoke-image-qt6
mise run script-smoke-image-edges-qt6
mise run script-smoke-image-lifecycle-edges-qt6
mise run script-smoke-image-level-first-frame-qt6
mise run script-smoke-image-builder-qt6
mise run script-smoke-image-builder-edges-qt6
mise run script-smoke-transform-edges-qt6
mise run script-smoke-toonz-raster-converter-qt6
mise run script-smoke-toonz-raster-converter-level-qt6
mise run script-smoke-toonz-raster-converter-edges-qt6
mise run script-smoke-toonz-raster-converter-lifecycle-edges-qt6
mise run script-smoke-outline-vectorizer-qt6
mise run script-smoke-centerline-vectorizer-qt6
mise run script-smoke-vectorizer-edges-qt6
mise run script-smoke-binding-lifecycle-edges-qt6
mise run script-smoke-rasterizer-qt6
mise run script-smoke-rasterizer-edges-qt6
mise run script-smoke-renderer-qt6
mise run script-smoke-renderer-frames-columns-qt6
mise run script-smoke-renderer-vector-qt6
mise run script-smoke-renderer-edges-qt6
mise run script-smoke-renderer-lifecycle-edges-qt6
mise run script-smoke-level-transformers-qt6
mise run script-smoke-wrapper-id-qt6
mise run script-smokes-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-run-errors-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-coercion-qt6
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
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-loadlevel-sequence-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-save-reopen-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-reload-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-load-failure-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-save-icon-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-save-icon-variants-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-frameids-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-io-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-io-types-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-path-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-path-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-level-first-frame-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-builder-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-builder-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-transform-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-level-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-outline-vectorizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-centerline-vectorizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-vectorizer-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-binding-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-rasterizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-rasterizer-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-frames-columns-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-vector-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-renderer-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-transformers-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-wrapper-id-qt6
mise run script-smokes-natural-exit-qt6
```

Manual smoke checks:

- launch from the build tree
- launch from packaged artifact
- create a new scene
- open an existing scene
- draw raster and vector strokes
- scrub xsheet/timeline
- use onion skin
- render preview and final frame
- run a script fixture
- record and play audio
- use camera preview and still capture
- validate tablet/stylus input on Windows

## Final Answer On Metal Sequencing

Park Metal follow-up until the Qt 6 migration is complete. The Metal draft did
not reach visual parity, so the Qt 6 port should proceed directly against the
current OpenToonz behavior, including viewer, drawing, rendering, and
visual-parity work. Revisit Metal later using the completed Qt 6 port as the
baseline.
