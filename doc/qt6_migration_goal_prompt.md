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

As of June 2, 2026, the initial Qt 6 runway is already implemented in this
branch.

- `OPENTOONZ_QT_MAJOR` exists and defaults to the Qt 5 lane.
- Qt target, MOC, resource, and translation command selection is centralized.
- Nix, CMake presets, and mise tasks include a Qt 6 lane next to the Qt 5 lane.
- A dedicated GitHub Actions workflow,
  `.github/workflows/workflow_qt6_experimental_binaries.yml`, builds
  experimental Qt 6 binary artifacts for Linux, Windows, and macOS. On
  `bgyss/opentoonz`, it runs weekly on Mondays at 10:00 UTC, on manual
  dispatch, and on the `qt-6-experimental` tag, explicitly checking out
  `codex/qt-6-port` for scheduled/manual runs. It labels artifacts with
  `qt-6-experimental` and publishes a rolling prerelease with that tag.
  The old `qt6-experimental` tag remains accepted as a compatibility trigger.
  `mise run release-qt6-experimental` retags the current clean branch and
  pushes the tag to refresh that rolling prerelease.
- `mise run check-windows-msvc-abi` provides a fast local guard for Windows
  MSVC DLL import/export annotation hazards that native macOS/Linux builds can
  miss. The guard also runs before the normal `mise run build`,
  `mise run build-qt6`, and `mise run check` tasks.
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
- `mise run check-core5compat-scope` now enforces that direct `QTextCodec`
  usage stays inside `ttextcodec.h`, that only the documented adapter consumers
  include it, and that Core5Compat references stay in the adapter-owning CMake
  targets. The guard uses explicit C++ identifier-boundary matching so removed
  Qt 5 API tokens cannot slip past the grep check.
- `mise run check-qt6-fontmetrics-scope` now enforces that direct deprecated
  `QFontMetrics::width()` calls stay out of feature code and remain behind
  `QtCompat::fontMetricsHorizontalAdvance()`.
- `mise run check-qt6-script-smoke-registry` now verifies that every task in
  `scripts/qt6/run-all-script-smokes.sh` exists in `mise.toml` and that every
  `OPENTOONZ_SCRIPT_FIXTURE` / `OPENTOONZ_SCRIPT_LIBRARY_FIXTURE` path
  referenced by the script-smoke tasks exists on disk. It also fails if a new
  top-level `.toonzscript` fixture under `toonz/sources/tests/scriptengine`
  is not registered in `mise.toml`, except for the documented default
  `basic.toonzscript` and its default child helper `run_child.toonzscript`.
  This keeps new QJSEngine compatibility fixtures from silently drifting out
  of the aggregate smoke suite.
- Direct removed desktop-widget APIs are no longer present under
  `toonz/sources`. `mise run check-qt6-desktopwidget-scope` now keeps
  `QDesktopWidget`, `QApplication::desktop()`, `qApp->desktop()`, and direct
  generic `desktop()` calls out of the source tree so screen geometry work
  stays on `QScreen`/widget-screen helpers. Multi-monitor, mixed-DPI, and
  off-primary-screen behavior still need manual runtime verification.
- `mise run check-no-qregexp` and `mise run check-core5compat-scope` now also
  run before the normal local configure, build, Qt 6 configure, and Qt 6
  translation-build tasks, so removed regex APIs and adapter-scope regressions
  fail before CMake/Nix work starts.
- Core5Compat is no longer part of the global `${QT_CORE_LINK_TARGETS}` set.
  It is now exposed as `${QT_CORE5COMPAT_LINK_TARGETS}` and linked only by
  `image`, `toonzlib`, and `OpenToonz`, the current targets that include the
  legacy text-codec adapter. Drop those targeted links once the adapter no
  longer needs Core5Compat.
- An isolated Qt 6 translation lane exists through the
  `nix-qt6-translation-check` CMake preset,
  `mise run configure-qt6-translations`, and
  `mise run build-qt6-translations`. The build task keeps
  `WITH_TRANSLATION=ON` verification separate from the default Qt 5 lane and
  the normal Qt 6 build directory, resolves Qt 6 `lrelease` through the
  imported `Qt6::lrelease` target, and verifies that the expected 63 generated
  `.qm` files are produced under `toonz/stuff/config/loc`.
- Translation generation now has build evidence, but packaged runtime language
  switching still needs manual verification before claiming release-ready
  localization parity.
- After that link-scope reduction, both app targets still build:
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel` and `cmake --build toonz/build/nix-relwithdebinfo --target
  OpenToonz --parallel`.
- On June 5, 2026, `mise run build-qt6` passed against the current branch with
  the expanded local preflight chain enabled: Windows MSVC ABI, QRegExp,
  Core5Compat scope, multimedia scope, script scope, font metrics scope, and
  high-DPI startup attribute scope. Apple builds now define
  `GL_SILENCE_DEPRECATION` at the CMake compile-definition boundary, removing
  known legacy OpenGL/GLUT deprecation-warning noise without changing the active
  rendering path.
- Direct checkbox state-change connects in `toonz`, `toonzqt`, shared headers,
  stop-motion, and Type tool options now route through
  `QtCompat::connectCheckStateChanged`. Qt 6.7+ uses
  `QCheckBox::checkStateChanged`, while the Qt 5 lane keeps the existing
  `stateChanged` signal behind the helper. `mise run
  check-qt6-checkbox-state-scope` keeps direct `QCheckBox::stateChanged`
  connects out of feature code.
- Legacy `QButtonGroup` integer button-signal connects now route through
  `QtCompat::connectButtonGroupIdClicked()` and
  `QtCompat::connectButtonGroupIdPressed()`. Both supported lanes use the non-deprecated `idClicked` /
  `idPressed` signals behind the helpers. `mise run check-qt6-buttongroup-scope` keeps direct legacy
  `buttonClicked(int)` / `buttonPressed(int)` connects out of feature code.
- The active `QtOfflineGL` offscreen context path now uses `QSurfaceFormat`
  instead of an unused legacy `QGLFormat` setup block. `mise run
  check-qt6-qglformat-scope` keeps `QGLFormat` limited to the remaining Qt
  5-only startup/PBuffer compatibility scope and verifies active references
  remain inside Qt 5-only preprocessor branches.
- `mise run check-qt6-qgllegacy-scope` keeps `QGLContext` and
  `QGLPixelBuffer` limited to the documented Qt 5-only OpenGL compatibility
  scope, so active Qt 6 paths stay on `QOpenGLContext`, `QOffscreenSurface`,
  and `QOpenGLFramebufferObject`.
- `QWheelEvent` pixel-delta access is now centralized behind
  `QtCompat::wheelEventPixelDelta()` alongside the existing wheel position and
  angle-delta helpers. `mise run check-qt6-wheelevent-scope` keeps direct
  `QWheelEvent::pixelDelta()` calls out of feature code.
- Schematic graphics-scene spin/aim handle drags now route screen-position
  deltas through `QtCompat::graphicsSceneMouseEventScreenPosition()` and
  `QtCompat::graphicsSceneMouseEventLastScreenPosition()`. `mise run
  check-qt6-graphicssceneevent-scope` keeps direct `screenPos()` /
  `lastScreenPos()` calls out of feature code.
- The recent flipbook image loader no longer uses Qt 6-deprecated
  `QAction::parentWidget()` and now casts the action `parent()` instead.
  `mise run check-qt6-qaction-scope` guards that QAction parent-widget warning
  slice.
- Direct feature-code `QImage::mirrored()` usage is now centralized behind
  `QtCompat::mirroredImage()`. Qt 6 uses `QImage::flipped()` with explicit
  orientations, while Qt 5 keeps the existing `mirrored()` behavior behind the
  helper. `mise run check-qt6-qimage-mirrored-scope` keeps direct
  `QImage::mirrored()` calls out of feature code.
- A small event-coordinate compatibility slice is now centralized in
  `QtCompat`: selected `QMouseEvent` position/global-position and
  `QDropEvent`-derived drag/drop position users now call Qt 6
  `position()`/`globalPosition()` APIs while preserving Qt 5 behavior.
  Synthetic `QMouseEvent` construction for Xsheet auto-pan and fake
  context-menu releases also routes through `QtCompat::makeMouseEvent()` with
  explicit local/global positions, and `PlaneViewer` mouse press/move handling
  no longer uses deprecated `QMouseEvent::x()` / `y()` accessors. Shared
  `toonzqt` numeric fields, paired numeric range fields, spectrum key editing,
  color sample fields, swatch point editing/panning, scroll widgets,
  flip-console slider/split-button controls, Function Sheet row and column-head
  context-menu placement, Function Tree channel middle-drag initiation,
  mini-toolbar dragging, and dock hover/drag/resize/separator/
  drop-placeholder paths, plus Color Field context-menu placement, Style Name
  Editor word-button context-menu placement, Palette Viewer tab-bar
  context-menu placement, Palette Viewer page-chip hit testing, drag
  thresholding, rename activation, tab dragging, tab rename activation,
  page-drop targeting, palette page context-menu placement, palette page
  tooltip placement, palette-icon drag thresholding, and save-toolbar drop hit
  testing, plus Studio Palette tree drag thresholding, row hit testing,
  context-menu placement, and drop-target tracking, also use `QtCompat`
  event-position helpers instead of direct Qt 5-era local/global coordinate
  accessors. Schematic Viewer panning,
  zooming, rubber-band gating, double-click fit-scene hit testing, port
  link-start geometry, group-editor title-bar hit testing, and FX/stage
  schematic node rename double-click hit testing also use the same helpers.
  Spreadsheet panel panning, cell hit testing, and auto-pan
  edge tracking now use the same helpers. Style Editor color wheels, color
  sliders, parameter chips, style chooser chip hit testing, style chooser
  context-menu placement, and style chooser tooltip placement now use the same
  helpers. Custom Panel scroller drag thresholds now use the same helpers.
  Insert FX preset removal, Audio Recording popup, Filmstrip frame-panel,
  Pencil Test sub-camera preset, Command Bar, Xsheet Toolbar, Tool Properties
  panel, Command Bar customization tree, DvDirTreeView version-control tree,
  Menu Bar customization tree, room tabs, panel title-bar Safe Area/Preview
  menus, Viewer panel show/hide menu, Brush Preset panel show/hide menu, Layer
  Footer frames-per-page menu, Farm Server list, Camera Track preview, Palette
  Gizmo binding menu, Color Model viewer, task tree context-menu plumbing,
  `DvItemViewer` context-menu placement, and Xsheet PDF preview context-menu
  placement now also route through the same context-menu helpers.
  Room tab selection, reordering, and rename activation now use the same
  helpers. History pane undo/redo row selection now uses the same helpers.
  Camera Capture histogram range dragging, Camera Track export preview panning,
  Xsheet PDF export preview panning, Cleanup swatch panning, Separate Colors
  swatch panning, Color Model picking, Brush Preset item drag initiation and
  hotspot placement, Command Bar command-list hit testing, `TreeView` item hit
  testing, drag dispatch, and context-menu placement plus Ruler guide
  creation, movement, deletion, and hover hit testing plus Board Settings item
  hover and drag geometry plus floating panel title-bar close-button hover and
  click hit testing plus Startup scene-list hover hit testing plus
  `ToneCurveField` channel curve control-point editing plus `DvItemViewer`
  item selection, play-hit testing, middle-button panning, drag initiation,
  rename activation, table-column resize tracking, and item tooltip placement
  plus Xsheet keyframe mover click/drag cell mapping plus Pencil Test
  sub-camera handle hit testing and drag resizing plus measured-value field
  middle-drag editing also use the same helpers. Pencil Test capture image
  orientation flips now use Qt 6
  `QImage::flipped()` while preserving the Qt 5 `mirrored()` path. Filmstrip
  frame double-click selection, press selection, navigator panning initiation,
  inbetween-button hit testing, and drag/drop arming, plus drag-select,
  auto-pan edge tracking, inbetween tooltip hover checks, frame-head onion-skin
  toggles, frame-head current-frame movement, and shift-trace frame-head hover
  hit testing now use the same helpers. Xsheet row-area current-frame,
  onion-skin, shift-trace, navigation-tag, play-range, panning, auto-pan,
  marker removal, context-menu placement, and double-click onion toggle paths
  now use the same helpers. Xsheet column-area motion-path menu, object-change
  popup hover, column selection, toggle hit testing, panning, auto-pan,
  settings popup placement, double-click rename hit testing, synthetic
  context-menu release, and context-menu placement now use the same helpers.
  Xsheet cell-area note hit testing, cell/level drag initiation, sound-level
  handle hit testing, panning, auto-pan, tooltip targeting, double-click rename
  targeting, and context-menu placement now use the same helpers. Xsheet drag
  tools for generic cell mapping, selection drags, note movement, column
  selection/movement, sound volume editing, and drag/drop target mapping also
  use the same helpers. Xsheet level-cell move click/drag mapping now uses the
  same helpers. Screen Picker screen-rectangle press, drag, and release geometry
  and Image Viewer panning, color picking, context-menu placement, and
  loadbox/zoom drag setup now use the same helpers.
  SceneViewer
  mouse/tablet event initialization, tablet context-menu
  delivery, widget context-menu delivery, tablet hover-edge handling, mouse
  double-click mapping, GUI smoke system-event mouse diagnostics, synthetic
  smoke mouse-event construction, and the Function Panel graph/drag tools now
  use the same helpers, removing another Qt 6
  event-coordinate warning slice while preserving the Qt 5 lane.
- Plane and Swatch Viewer context-menu placement now also routes through
  `QtCompat` context-menu helpers instead of direct global-position accessors.
- Wheel-event zoom centers for the plane, swatch, schematic, Function Panel,
  Cleanup swatch, Separate Colors swatch, Image Viewer, SceneViewer, and
  Xsheet frame zoom, plus the optional straight-skeleton debugger wheel zoom,
  now use `QtCompat` wheel-position helpers instead of direct Qt 5-era
  `QWheelEvent` position/delta accessors.
- `DvTextEdit` mini-toolbar font-size population and text-family formatting no
  longer use Qt 6-deprecated `QFontDatabase` instance construction or
  `QTextCharFormat::setFontFamily()`. The code now uses APIs available in both
  Qt 5 and Qt 6. `mise run check-qt6-fontdatabase-scope` keeps those direct
  deprecated font APIs out of feature code, and verifies that the documented
  compatibility files only instantiate `QFontDatabase` inside Qt 5 fallback
  branches.
- Font parameter style lookup and preview font construction now route through
  `QtCompat` helpers. Qt 6 uses the static `QFontDatabase` APIs, while Qt 5
  keeps the instance-based API path.
- The lower-level `TFontManager` path in `common/tvrender/tfont_qt.cpp` no
  longer stores a heap `QFontDatabase` instance. Font family/style/private
  family/bold/italic queries now go through local Qt-version-aware helpers so
  the Qt 6 lane uses the static font database APIs while preserving the Qt 5
  lane.
- The `t32bitsrv` raster exchanger now makes its raw byte-copy intent explicit
  when writing into non-trivial pixel wrapper storage, removing the
  `-Wnontrivial-memcall` warning from the `tnzcore` frontier without changing
  the byte-stream exchange behavior.
- The raster codec header restore path and generic image-I/O pixel copy loop
  now also make their raw byte-copy destination intent explicit with
  `static_cast<void *>`, removing another pair of `-Wnontrivial-memcall`
  warnings from the `tnzcore` / `image` frontier without changing the serialized
  raster data layout or pixel-copy stride.
- Function Panel group keyframe handles now have named enum values
  (`GroupPoint`, `GroupSpeedOut`, and `GroupSpeedIn`) instead of casting and
  comparing magic `100` / `101` / `102` values against `FunctionPanel::Handle`.
  This removes the Qt 5/Qt 6 `-Wswitch` and tautological enum comparison warning
  cluster while preserving the existing handle values and group-keyframe UI
  behavior.
- Palette/style RTTI comparisons now resolve smart pointers to raw
  `TColorStyle` / `TPersist` pointers before calling `typeid`, removing the
  side-effect operand warning from `TPalette::setStyle()`,
  `TPersistSet::insert()`, and `SettingsPage::setStyle()` without changing
  dynamic-style replacement behavior.
- The 64-bit non-matte `TRop::ropmin()` path now uses the 64-bit raster loop
  macros and `TPixel64` temporary storage instead of accidentally re-entering
  the 32-bit loop. This removes the tautological 8-bit channel comparison
  warning and keeps the 16-bit blend path on 16-bit pixel types.
- The RGB blur column writer now selects its backlit crop constant at compile
  time from the pixel channel size, so 8-bit builds no longer instantiate the
  16-bit crop assignment that produced `-Wconstant-conversion` noise. The
  existing 8-bit `204` and 16-bit `204 * 257` crop values are unchanged.
- The legacy OpenToonz 4.6 raster release wrapper no longer compiles an
  unsupported `delete` of `_RASTER::buffer`, which is stored as `void *`.
  Buffer ownership remains with the modern smart raster objects; unsupported
  buffer-release requests still trip the existing assertion.
- The legacy `flt_w_1` resample filter value is now named as `TRop::W1 = 101`
  instead of appearing as a raw numeric switch case. `TRop::HowMany` keeps its
  previous count value, and the old filter function/radius mapping is
  unchanged.
- The 64-bit proxied QuickTime movie-settings path now brings the application
  window back to the front through Qt `raise()` / `activateWindow()` instead
  of deprecated Carbon `SetFrontProcess`, removing that macOS deprecation
  warning without changing the 32-bit helper communication flow.
- The remaining abstract-final warning cluster in `tnzcore` has been resolved
  by restoring the Qt 5 `QtOfflineGLPBuffer` override for the shared-context
  `createContext()` interface and by completing the
  `TStrokeTwirlDeformation` control-point displacement overrides. This keeps
  both classes concrete instead of relying on stale final annotations.
- The `stdfx` / `tnztools` compile frontier now removes another warning slice:
  Mosaic and Motion Blur raw pixel buffer operations use explicit `void *`
  casts, Iwa seed ranges explicitly cross the `double` parameter boundary,
  Flow Paint Brush handles its `NoSort` enum value, `ChangeColorFx` implements
  the required `canHandle()` override, and raster-selection paste logic now
  compares `TImage::Type` with `TImage::RASTER` instead of an xsheet level
  enum.
- The `toonzqt` / `tnztools` warning frontier now also makes
  `ColumnToCurveMapper` safely deletable through its abstract base pointer,
  marks vector/stroke selection undo history methods as overrides, and ensures
  Style Picker property changes return a defined success value on every path.
- The `stdfx` Fractal Noise warning frontier now spells out all `FractalType`
  values in the conversion switch, including no-op base, dynamic,
  dynamic-twist, and sentinel cases, so the compiler can verify enum coverage
  without changing the existing conversion behavior.
- The macOS `TSystem::moveFileToRecycleBin()` path no longer uses deprecated
  Carbon `FSFindFolder` / `FSRefMakePath` trash-folder lookup. It resolves the
  current user's `~/.Trash` through Qt and keeps the existing rename/copy/delete
  fallback behavior.
- Stop-motion and legacy Pencil Test sizing code now routes font text
  measurement through `QtCompat::fontMetricsHorizontalAdvance()`, so Qt 6 avoids
  deprecated `QFontMetrics::width()` while the Qt 5 lane keeps the compatible
  fallback.
- Configure Shortcuts multi-key conflict checking no longer relies on the
  Qt 6-deprecated implicit `QKeyCombination` to `int` conversion. The Qt 6 lane
  now uses `QtCompat::keySequenceEntryToInt()`, which calls
  `QKeyCombination::toCombined()` on Qt 6 while the Qt 5 lane keeps the
  existing integer key-sequence values. `mise run
  check-qt6-qkeysequence-scope` keeps direct `toCombined()` calls inside
  `QtCompat`.
- Audio Recording and Auto Lip Sync media preview source/state handling now use
  `QtCompat::setMediaPlayerSource()` and `QtCompat::mediaPlayerState()`, so the
  Qt 6 lane uses `QMediaPlayer::setSource()` / `playbackState()` in one shared
  boundary while the Qt 5 lane keeps `setMedia()` / `state()`. `mise run
  check-qt6-mediaplayer-scope` keeps those direct Qt 6 `QMediaPlayer` source
  and state calls inside `QtCompat`.
- Separate Colors color-string defaults and settings restoration no longer call
  Qt 6-deprecated `QColor::setNamedColor()`. Stored color strings now parse
  through `QColor(QString)`, which keeps the Qt 5 lane compatible and removes
  the direct deprecated call from the current app/UI/header tree. `mise run
  check-qt6-qcolor-scope` keeps direct `QColor::setNamedColor()` calls out of
  feature code.
- Qt Script/QJSEngine color argument parsing and OutlineVectorizer transparent
  color assignment also use `QColor(QString)` instead of the deprecated
  `QColor::setNamedColor()` call.
- Touch gesture code in the plane, swatch, schematic, image, and scene viewer
  paths now uses `QtCompat` touch-point helpers. Qt 6 uses
  `QTouchEvent::points()` with `QEventPoint::position()` / `lastPosition()`,
  while Qt 5 keeps `touchPoints()` with `QTouchEvent::TouchPoint::pos()` /
  `lastPos()`. Touch device type checks also route through
  `QtCompat::touchDeviceType()`, keeping the Qt 6 `QInputDevice` and Qt 5
  `QTouchDevice` type boundary in one place. `mise run check-qt6-touch-scope`
  keeps direct `QTouchEvent::touchPoints()` and `device()->type()` access
  inside `QtCompat`.
- Direct tablet-event `posF()` / `globalPos()` access and direct
  `QTabletEvent` construction are guarded by
  `mise run check-qt6-tabletevent-scope`, keeping those paths behind
  `QtCompat::tabletEventPositionF()`,
  `QtCompat::tabletEventGlobalPosition()`, and
  `QtCompat::makeTabletEvent()`. The Windows pointer-input bridge now creates
  synthetic tablet events through that helper; Qt 6 uses a synthetic
  `QPointingDevice`, while Qt 5 keeps the existing `QTabletEvent` constructor
  path. This is source-level guard coverage; real hardware pressure/tilt
  validation remains manual.
- Direct mouse/context/drop-event coordinate access is now guarded by
  `mise run check-qt6-mouseevent-scope`, keeping stale `x()`, `y()`, `pos()`,
  `globalPos()`, `globalX()`, and `globalY()` event accessors from
  re-entering common handler code after the broad `QtCompat` coordinate-helper
  migration. The guard also removed obsolete commented examples that still
  showed direct `QMouseEvent::pos()` usage.
- Deprecated `QVariant::canConvert(QVariant::...)` overloads have been removed
  from the current app/UI/header tree. Fixed-type settings restore paths use
  templated `canConvert<T>()` checks, and dynamic preference type checks route
  through a version-aware helper that uses `QMetaType` on Qt 6 while preserving
  the Qt 5 `QMetaType::Type` path. `mise run check-qt6-qvariant-scope` keeps
  non-template `canConvert(...)` calls inside that helper.
- The Qt 6 startup path no longer sets the removed/deprecated high-DPI
  application attributes; Qt 6 uses its always-on high-DPI behavior, while the
  Qt 5 lane keeps the existing attributes. The translator startup path also
  uses `QLibraryInfo::path()` on Qt 6 and explicitly discards optional
  `QTranslator::load()` results. After this cleanup, the current app-target
  warning frontier in `main.cpp` is limited to OpenGL/GLUT deprecation output.
- `mise run check-qt6-highdpi-attribute-scope` now guards that startup cleanup:
  `AA_EnableHighDpiScaling` and `AA_UseHighDpiPixmaps` must remain inside
  `QT_VERSION < QT_VERSION_CHECK(6, 0, 0)` blocks. It runs before the normal
  local configure, build, and translation-build tasks.
- On June 10, 2026, macOS SDK detection in `nix/opentoonz-env.nix` and
  `scripts/nix/prepare-tiff.sh` was updated for macOS 26+ toolchains. The old
  logic only accepted `MacOSX15.sdk`/`MacOSX15.4.sdk` and used the removed
  `AGL.framework` as its detection marker, so a Command Line Tools update that
  dropped SDK 15 broke every local compile lane. Detection now validates any
  inherited `OPENTOONZ_MACOS_SDKROOT` (ignoring stale paths), uses
  `OpenGL.framework` plus `GLUT.framework` as the marker, and falls back to the
  default `MacOSX.sdk` locations. Because Qt 5.15's static UiTools `.prl`
  still lists `-framework AGL`, the Qt 5 lane seeds
  `_Qt5UiTools_RELEASE_AGL_PATH` from the new
  `OPENTOONZ_QT5_AGL_COMPAT_PATH` env var (real AGL when present, otherwise
  `OpenGL.framework` as a no-op stand-in; UiTools references no AGL symbols).
  The sandboxed flake `package`/`configureCheck` derivations seed the same
  cache variable directly on their `cmake` command line. `AGL/agl.h` is only
  included by dead `LEVO_MACOSX` code, so no active source path needs AGL.
  After wiping the stale SDK-15 build caches, both lanes fully rebuilt and
  linked against `MacOSX.sdk` (macOS 26), `mise run check` passes both
  sandboxed configure checks, `check-textcodec`/`check-textcodec-qt6` pass,
  and the full `mise run script-smokes-qt6` aggregate passes against the
  rebuilt Qt 6 app. GUI smokes after this SDK change still require
  `mise run package-macos-qt6` before rerunning.
- On June 10, 2026, the `gui-smoke-script-console-view-qt6` console command was
  updated for the strict binding argument validation added on June 5. The smoke
  used the legacy two-argument `new Level("Raster", "name")` form, which the
  legacy Qt Script constructor silently ignored (creating an empty level); the
  strict QJSEngine bootstrap now rejects more than one argument, so the smoke
  constructs `new Level()` directly, preserving the empty-level-then-`setFrame`
  behavior it always exercised. The smoke passes again after the fix.
- On June 10, 2026, a race in `gui-smoke-preview-render-output-qt6` was fixed
  in both lanes. The smoke fired level/xsheet/scene change notifications and
  started the Previewer render after only a 100 ms event pump, but the
  Previewer invalidates cached frames through 300 ms debounce timers hooked to
  those notifications; when the debounced invalidation fired after the render
  completed, it cleared the freshly cached frame (raster pixels zeroed, frame
  no longer ready) even though the listener saw a completion callback —
  `Previewer::Imp::doOnRenderRasterFailed` also reports failures through
  `notifyCompleted`, and `notifyFailed` is never called, so listeners cannot
  distinguish those outcomes. The smoke now pumps 500 ms past the debounce
  window before rendering, records frame-ready/raster stats at
  completion-callback time in the failure output, and
  `Previewer::Imp::onRenderFailure` now logs the failed frame and exception
  message instead of failing silently. The preview smoke passed five
  consecutive packaged Qt 6 runs after the fix.
- On June 10, 2026, after the macOS 26 SDK migration and the two smoke fixes
  above, a full validation sweep passed on the rebuilt branch: all 25 Qt 6
  scope guards, `check-textcodec`/`check-textcodec-qt6`, `mise run check`
  (both sandboxed flake configure checks), full Qt 5 and Qt 6 lane builds,
  `package-macos-qt6`, `check-macos-arm64-qt6` (291 arm64 Mach-O files),
  the complete `gui-smokes-app-qt6` aggregate (63 packaged app-side GUI
  smokes), `script-smokes-qt6`, and `script-smokes-natural-exit-qt6`
  (55 fixtures). Hardware camera/audio, real OS-level input delivery, and
  Qt 5-vs-Qt 6 visual parity items remain open as before.
- Legacy `toonzfarm/tfarm/tbaseserver.cpp` Windows socket diagnostics no
  longer use unbounded `wsprintf()` calls; the messages now use bounded
  `snprintf()` formatting, and the non-Windows send failure message is
  initialized before throwing.
- A bounded Qt 6 Cocoa startup smoke has been attempted with an isolated runtime
  `stuff` copy. It stayed alive until stopped by the smoke harness, but exposed
  plugin discovery gaps.
- Runtime plugin wiring is now in place for the current macOS Qt 6 lane: the
  Nix Qt plugin path includes QtSvg, macOS packaging recognizes Qt 6
  `multimedia` plugins plus SVG `iconengines`, and packaged plugins are
  rewritten to use bundled Qt frameworks instead of Nix-store Qt frameworks.
- Stop-motion capture-sound playback now uses `QSoundEffect` in both Qt lanes,
  removing the old Qt 5-only `QSound` branch while preserving the camera snap
  sound feature.
- `mise run check-qt6-multimedia-scope` now guards the current multimedia API
  boundary: `QSound` must not reappear, and Qt 5 video-surface APIs must remain
  confined to the legacy 32-bit Qt 5 `penciltestpopup_qt.*` fallback. The
  `toonz` CMake source split now explicitly keeps that fallback out of every
  Qt 6 lane by selecting the modern stop-motion/Pencil Test sources whenever
  `OPENTOONZ_QT_MAJOR` is 6, even if an unusual non-64-bit Qt 6 configuration
  is attempted. The active Qt 6 Pencil Test path avoids that video-surface API
  and still needs real live-camera workflow validation. The guard also runs
  before the normal local configure, build, translation-build, and check tasks
  so multimedia API regressions fail early.
- `mise run check-qt6-fontmetrics-scope` now guards the direct
  `QFontMetrics::width()` warning slice by allowing that deprecated API only in
  the Qt 5 fallback inside `QtCompat::fontMetricsHorizontalAdvance()`. It runs
  before the normal local configure, build, and translation-build tasks.
- User-activated real `QComboBox` index handling now routes through
  `QtCompat::connectComboBoxActivatedIndex()`, which uses `textActivated` and
  forwards the selected current index. The cleanup covers the stop-motion
  Pencil Test popup, Camera Track export popup, cleanup settings panes,
  project selectors, tool option combos, board settings, Xsheet PDF export,
  Filmstrip, level settings, Xsheet column filters, histogram/function segment
  widgets, file browser DPI policy, format settings, and navigation-tag color
  selector. Project-local custom `ToolOptionPopupButton::activated(int)`
  signals remain on their custom signal path. `mise run
  check-qt6-combobox-activated-scope` keeps direct real-combo
  `QComboBox::activated` connections and old macro `SIGNAL(activated(int))`
  combo connections out of the current source tree and runs before the normal
  local configure, build, and translation-build tasks.
- On June 5, 2026, targeted app-target rebuilds after the combo-box activation,
  QImage mirrored/flipped, checkbox state-change, QWheelEvent pixel-delta, and
  QGLFormat scope slices passed in both lanes:
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel` and
  `cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz
  --parallel`. The Qt 5 run became a broad rebuild after Ninja recovered its
  build log, but still linked successfully. The remaining output was existing
  macOS OpenGL and legacy compiler warning noise.
- On June 5, 2026, `mise run script-smokes-qt6` passed against the current Qt 6
  app bundle, covering the aggregate FilePath, Scene, Level, Image,
  ImageBuilder, Transform, ToonzRasterConverter, vectorizer, Rasterizer,
  Renderer, lifecycle, and wrapper-id fixture set.
- On June 5, 2026, `mise run script-smokes-natural-exit-qt6` also passed
  across the same fixture set with `OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1`,
  including the QApplication-backed Rasterizer and Renderer fixtures.
- `mise` includes Qt 6 macOS packaging and arm64 bundle-check tasks:
  `package-macos-qt6` and `check-macos-arm64-qt6`.
- `mise run package-macos-qt6` and `mise run check-macos-arm64-qt6` pass for
  the current macOS arm64 Qt 6 bundle; the 2026-05-31 post-GUI-relink check
  inspected 291 packaged arm64 Mach-O files.
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
  drawing, onion skin, tool overlays, manual FX Preview UI and broader FX
  preview workflows, or Qt 5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes
  `mise run gui-smoke-preview-render-output-qt6`. The app-side smoke creates a
  one-frame standard-DPI red raster scene, sets the preview render settings to
  320x240 without viewer shrink, renders frame 0 through the `Previewer`
  render-cache path, and verifies a ready 320x240 preview raster with
  `Red pixels: 76800`. This narrows the basic Previewer cache/render path; it
  does not yet validate the File > Preview UI, manual schematic FX Preview
  command path, sub-camera preview, flipbook controls, manual preview-save
  popup/overwrite/warning dialogs, production FX-heavy preview scenes, or Qt
  5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes
  `mise run gui-smoke-fx-preview-render-output-qt6`. The app-side smoke creates
  a one-frame standard-DPI red raster scene, starts
  `PreviewFxManager::showNewPreview()` on the column FX, verifies that a
  preview `FlipBook` is attached to the expected FX/xsheet, waits for the
  `renderedFrame` signal, and inspects the `PreviewFxManager` cache raster. It
  verifies a ready 320x240 preview cache image with `Red pixels: 76800`. This
  narrows the FX Preview manager/flipbook/cache render path; it does not yet
  validate the manual schematic context-menu command, live UI playback/timer
  behavior, manual save popup/overwrite/warning dialogs, production FX-heavy
  preview scenes, or Qt 5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes
  `mise run gui-smoke-fx-preview-subcamera-render-output-qt6`. The app-side
  smoke creates a one-frame standard-DPI red raster scene, enables Preview
  Properties sub-camera preview, sets a 160x120 interest rectangle inside a
  320x240 preview camera, starts `PreviewFxManager::showNewPreview()` on the
  column FX, verifies that the manager reports sub-camera preview active, and
  inspects the `PreviewFxManager` cache raster. It verifies a ready 160x120
  cropped preview cache image with `Red pixels: 6820`. This narrows the FX
  Preview sub-camera manager/cache path; it does not yet validate the manual
  schematic context-menu command, live UI sub-camera dragging, production
  FX-heavy preview scenes, or Qt 5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes
  `mise run gui-smoke-fx-preview-flipbook-controls-qt6`. The app-side smoke
  creates a two-frame standard-DPI raster scene, starts
  `PreviewFxManager::showNewPreview()` on the column FX, verifies both active
  preview cache frames, switches the preview `FlipBook` to frame 2, clones the
  preview, freezes it, verifies the cloned frozen cache frames, and unfreezes
  it. It verifies frame 0 as 320x240 with `Red pixels: 76800` and frame 1 as
  320x240 with `Green pixels: 76800`. This narrows FX Preview flipbook frame
  switching, clone, freeze, and unfreeze behavior; it does not yet validate the
  manual schematic context-menu command, live UI playback/timer behavior,
  manual save popup/overwrite/warning dialogs, production FX-heavy preview
  scenes, or Qt 5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes
  `mise run gui-smoke-fx-preview-save-previewed-frames-qt6`. The app-side smoke
  creates a two-frame standard-DPI raster scene, starts
  `PreviewFxManager::showNewPreview()` on the column FX, waits for both preview
  cache frames, saves the preview `FlipBook` to a PNG sequence through
  `FlipBook::doSaveImages()` with the test-safe completion notification
  suppressed, reloads both exported PNGs through Qt image I/O, and verifies two
  320x240 output frames with `Frame 0 red pixels: 76800` and
  `Frame 1 green pixels: 76800`. This narrows FX Preview saved-frame export for
  a simple two-frame raster scene; it does not yet validate the manual
  schematic context-menu command, live UI playback/timer behavior, manual save
  popup/overwrite/warning dialogs, production FX-heavy preview scenes, or Qt
  5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes
  `mise run gui-smoke-fx-preview-subcamera-save-previewed-frames-qt6`. The
  app-side smoke creates a two-frame standard-DPI raster scene, enables Preview
  Properties sub-camera preview, sets a 160x120 interest rectangle inside a
  320x240 preview camera, starts `PreviewFxManager::showNewPreview()` on the
  column FX, waits for both preview cache frames, saves the preview `FlipBook`
  to a PNG sequence through `FlipBook::doSaveImages()` with the test-safe
  completion notification suppressed, reloads both exported PNGs through Qt
  image I/O, and verifies two 160x120 cropped output frames with
  `Frame 0 red pixels: 6820` and `Frame 1 green pixels: 6820`. This narrows FX
  Preview sub-camera saved-frame export for a simple two-frame raster scene; it
  does not yet validate the manual schematic context-menu command, live UI
  sub-camera dragging, manual save popup/overwrite/warning dialogs, production
  FX-heavy preview scenes, or Qt 5-vs-Qt 6 visual parity.
- On 2026-05-31, the packaged Qt 6 app now passes seven focused final-render
  PNG-output smokes:
  `mise run gui-smoke-final-render-output-qt6` and
  `mise run gui-smoke-final-render-background-output-qt6`, plus
  `mise run gui-smoke-final-render-sequence-output-qt6` and
  `mise run gui-smoke-final-render-composite-output-qt6`, plus
  `mise run gui-smoke-final-render-vector-output-qt6`, plus
  `mise run gui-smoke-final-render-toonz-raster-output-qt6`, plus
  `mise run gui-smoke-final-render-fx-output-qt6`. The first app-side smoke
  creates a one-frame standard-DPI red raster scene, sends it through
  `MovieRenderer` to a PNG sequence under the isolated GUI smoke root, reloads
  the PNG with Qt image I/O, and requires a 320x240 nonempty output with
  visible red pixels. The second smoke renders a partial red raster over an
  opaque blue scene background and requires both red foreground and blue
  background pixels. The third smoke renders two xsheet rows through
  `MovieRenderer` and verifies a two-frame 320x240 PNG sequence with a red
  first frame and green second frame. The fourth smoke renders two raster
  levels in separate xsheet columns and verifies red and green pixel columns in
  the composited 320x240 PNG output. The vector smoke renders a red vector level
  through `MovieRenderer` and verifies red pixels in the 320x240 PNG output.
  The Toonz Raster smoke creates a color-mapped `TZP_XSHLEVEL` frame with a red
  palette style, renders it through `MovieRenderer`, and verifies red pixels in
  the 320x240 PNG output. The FX smoke wraps a partial red raster scene in a
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
- On 2026-05-31, the packaged Qt 6 app also passes
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
  Qt widget events; it does not validate real OS-level input delivery, broader
  timeline onion workflows, or full visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
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
- On 2026-05-31, the packaged Qt 6 app also passes
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
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-onion-skin-context-menu-qt6`. This app-side smoke
  creates a five-frame raster scene, builds the onion-skin command menu through
  `OnioniSkinMaskGUI::addOnionSkinCommand`, triggers activate, deactivate,
  extend to scene, limit to level, clear fixed markers, clear relative markers,
  and clear all markers, then requires `onionSkinMenuProbe=ok`,
  `onionSkinProbe=ok`, `xsheetRowAreaProbe=ok`,
  `xsheetRowAreaHighDpiProbe=ok`, `viewerRenderProbe=ok`, high-DPI viewer
  probes, changed row-area pixels, visible onion pixels, and visible
  current-frame red pixels. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, current row `3`,
  onion rows `0,1,2`, MOS count `3`, FOS count `0`, MOS range `-3,-1`,
  activate/deactivate/extend/limit/clear fixed/clear relative/clear all command
  status `ok`, row-area changed pixels `820`, row-area non-background pixels
  `106716`, onion pixels `0 -> 91203`, `52610` changed viewer pixels, and
  `45651` red pixels. The packaged Qt 5 run matched those measurements. This
  covers app-side onion-skin context-menu command behavior through Qt actions;
  real OS-level right-click/menu delivery, broader timeline onion workflows, and
  full visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app also passes
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
- On 2026-05-31, the packaged Qt 6 app also passes
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
- On 2026-05-31, the packaged Qt 6 app also passes
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
  ruler UI, OS/menu interaction, safe-area variants beyond the separate
  preset/color and custom-file smokes, field-guide variants beyond the separate
  settings smoke, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-safe-area-presets-qt6`. This app-side smoke
  creates a blank sandbox scene, disables camera, field-guide, ruler, guide, and
  safe-area overlays, captures the `SceneViewer`, enables the default
  `PR_safe` safe-area preset, switches to the bundled custom
  `150MT_FR_PR_safe` preset, and requires `safeAreaPresetProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, stored safe-area preset
  name, visible default red safe-area pixels, visible custom red/green/blue
  safe-area pixels, changed green and blue pixels between presets, and saved
  captures. The packaged Qt 6 run reported logical viewer size `1006x831`, DPR
  `2` / `2.00`, framebuffer `2012x1662`, preset
  `PR_safe -> 150MT_FR_PR_safe`, default red pixels `5279`, default changed red
  pixels `5279`, custom red pixels `4884`, custom green pixels `2873`, custom
  blue pixels `3008`, preset-delta changed pixels `16043`, preset-delta green
  pixels `2873`, preset-delta blue pixels `3008`, and final red pixels `4884`.
  The packaged Qt 5 run matched those measurements. This covers app-side
  safe-area preset selection, bundled custom-color parsing, and viewer
  framebuffer rendering; it does not validate real title-bar context-menu
  delivery, persistent user-managed safe-area file lifecycle, OS/menu
  interaction, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-safe-area-custom-file-qt6`. This app-side smoke
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
  smoke config root; it does not validate real title-bar context-menu delivery,
  persistent user-profile file management outside the smoke root, OS/menu
  interaction, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-field-guide-settings-qt6`. This app-side smoke
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
  interaction, custom guide positions, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
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
- On 2026-05-31, the packaged Qt 6 app also passes
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
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-ruler-guide-lines-qt6`. This app-side smoke
  creates a blank sandbox scene, seeds two horizontal and two vertical guides at
  custom world positions, captures the `SceneViewer` with guides disabled,
  enables the guide overlay, then moves the guides and captures again. It
  requires `rulerGuideLineProbe=ok`, `viewerRenderProbe=ok`, high-DPI
  framebuffer probes, custom guide counts, changed neutral guide-line pixels,
  vertical and horizontal dashed guide-line segments, expected guide-position
  bands, moved-guide framebuffer deltas, two visible ruler widgets, and saved
  captures. The packaged Qt 6 run reported logical viewer size `994x819`, DPR
  `2` / `2.00`, framebuffer `1988x1638`, guide positions H
  `-96.00,132.00 -> -156.00,48.00` and V
  `-88.00,116.00 -> -136.00,56.00`, first guide-line neutral pixels `3393`,
  second guide-line neutral pixels `3394`, two detected guide columns and two
  guide rows in each capture, dashed segment counts V/H `702/993` then
  `703/992`, moved guide-line neutral pixels `3681`, `6785` changed pixels, and
  `0` red pixels. The packaged Qt 5 run matched those measurements. This covers
  app-side custom guide positions and dashed viewer guide-line rendering; it
  does not validate real OS-level input delivery, every manual ruler-guide
  variant, guide-line visual variants outside this app-side dashed-line guard,
  or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-ruler-ticks-qt6`. This app-side smoke creates a
  visible raster scene, disables the guide overlay, enables the ruler overlay,
  captures the horizontal and vertical `Ruler` widgets before and after a viewer
  zoom/pan transform, and requires `rulerTickProbe=ok`,
  `rulerWidgetHighDpiProbe=ok`, `rulerTransformProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer probes, visible ruler tick
  pixels, changed ruler tick pixels, saved ruler widget captures, visible red
  viewer pixels, and before/after `SceneViewer` captures. The packaged Qt 6 run
  reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, two visible ruler widgets, horizontal ruler tick pixels
  `4098 -> 4074`, vertical ruler tick pixels `3410 -> 3374`, horizontal changed
  ruler pixels `224`, vertical changed ruler pixels `236`, ruler units
  `124.2500 -> 180.1625`, `1425551` viewer changed pixels, and `747490` red
  pixels. The packaged Qt 5 run matched those measurements. This covers
  app-side ruler widget tick rendering and high-DPI widget capture after a
  viewer transform; it does not validate real OS-level input delivery,
  remaining manual ruler-guide variants beyond app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, or full overlay visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-ruler-guide-styles-qt6`. This app-side smoke
  creates a blank sandbox scene, seeds one horizontal and one vertical guide,
  enables the ruler widgets, captures both `Ruler` widgets, applies a custom
  QSS block through the existing `qproperty-ParentBGColor`,
  `qproperty-ScaleColor`, `qproperty-HandleColor`, and
  `qproperty-HandleDragColor` path, enables the guide overlay, and requires
  `rulerStyleProbe=ok`, `rulerWidgetHighDpiProbe=ok`,
  `viewerRenderProbe=ok`, high-DPI framebuffer and widget captures, custom
  ruler background/handle/scale pixels, changed ruler widget pixels, visible
  ruler widgets, guide counts, and changed neutral guide pixels. The packaged
  Qt 6 run reported logical viewer size `994x819`, DPR `2` / `2.00`,
  framebuffer `1988x1638`, horizontal ruler style pixels: background
  `0 -> 43570`, handle `0 -> 48`, scale `124`, changed `47712`; vertical
  ruler style pixels: background `0 -> 35858`, handle `0 -> 48`, scale `136`,
  changed `39312`; `1697` changed neutral guide pixels and `0` red pixels. The
  packaged Qt 5 run matched those measurements. This covers the app-side ruler
  QSS/qproperty style path and guide-overlay coexistence; it does not validate
  real OS-level input delivery, remaining manual ruler-guide variants beyond
  app-side create/move/delete/drag-hide, guide-line visual variants beyond
  app-side custom-position dashed-line coverage, or full overlay visual parity.
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
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-handles-qt6`. This app-side smoke
  creates the same visible raster scene, switches the current tool to `T_Edit`
  on `Col1`, sets the active axis to `All`, hovers the rotation handle, then
  drags the rotation, scale, and center handles through `SceneViewer` Qt mouse
  events using the viewer DPR. It requires `mouseEventProbe=ok`,
  `handleHoverProbe=ok`, `handleHitTestProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, changed rotation/scale/center values, changed
  hover pixels, changed final pixels, visible red content pixels, and
  before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, active axis `All`,
  handle unit `0.8585`, rotation angle `0.0000 -> -44.5595`, scale
  `1.0000 -> 1.5039`, center `0.0000,0.0000 -> 0.3863,0.1932`, `570` hover
  changed pixels, `2185` final changed pixels, and `357521` red pixels. The
  packaged Qt 5 run matched those measurements. This covers app-side
  Animate/Edit rotation, scale, and center handle hover and hit-test dragging
  through Qt mouse events; it does not validate real OS-level input delivery,
  cursor artwork variants beyond the separate mode cursor smoke, or full manual
  transform workflow visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-handle-variants-qt6`. This app-side
  smoke creates the same visible raster scene, switches the current tool to
  `T_Edit` on `Col1`, sets the active axis to `All`, then drives the All-mode
  fallback translation path plus the ScaleXY and Shear handles through
  `SceneViewer` Qt mouse events using the viewer DPR. It requires
  `mouseEventProbe=ok`, `handleVariantProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, changed fallback translation X/Y, changed
  ScaleXY X/Y values, changed Shear X/Y values, visible red content pixels, and
  before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, fallback translation
  `0.0000,0.0000 -> 0.4829,-0.5634`, ScaleXY
  `1.0000,1.0000 -> 1.1534,1.0588`, Shear
  `0.0000,0.0000 -> -0.2146,0.1545`, `58364` changed pixels, and `368866`
  red pixels. The packaged Qt 5 run matched those measurements. This covers
  app-side Animate/Edit All-mode fallback translation, nonuniform ScaleXY, and
  Shear handle variants through Qt mouse events; it does not validate real
  OS-level input delivery, full cursor artwork variants, or full manual
  transform workflow visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-axis-drags-qt6`. This app-side smoke
  creates the same visible raster scene, switches the current tool to `T_Edit`
  on `Col1`, then separately sets Position, Rotation, Scale, Shear, and Center
  active-axis modes and drags each through `SceneViewer` Qt mouse events using
  the viewer DPR. It requires `axisDragProbe=ok`, `mouseEventProbe=ok`,
  `viewerRenderProbe=ok`, changed stage-object X/Y, angle, scale, shear, and
  center values, high-DPI framebuffer probes, visible red content pixels, and
  before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, Position
  `0.0000,0.0000 -> 2.7042,-1.3521`, Rotation `0.0000 -> -44.5595`, Scale
  `1.0000 -> 1.5039`, Shear `0.0000,0.0000 -> -0.2146,-0.1545`, Center
  `0.0000,0.0000 -> 0.3863,0.1932`, `3099` changed pixels, and `358145` red
  pixels. The packaged Qt 5 run matched those measurements. This covers
  app-side Animate/Edit active-axis transform dragging for the five primary
  axis modes through Qt mouse events; real OS-level input delivery, full cursor
  artwork variants, and full manual transform workflow visual parity remain
  open.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-animate-tool-cursors-qt6`. This app-side smoke
  creates the same visible raster scene, switches the current tool to `T_Edit`
  on `Col1`, sends Qt mouse-move events through `SceneViewer` for Position,
  Rotation, Scale, Shear, and Center active-axis modes, and requires
  `animateCursorProbe=ok`, `mouseEventProbe=ok`, `viewerRenderProbe=ok`,
  expected normal cursor IDs, Alt precise-decoration cursor IDs, cursor pixmap
  availability for every checked cursor, high-DPI framebuffer probes, visible
  red content pixels, and before/after captures. The packaged Qt 6 run reported
  logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`,
  Position cursor `23/2097175`, Rotation cursor `33/2097185`, Scale cursor
  `64/2097216`, Shear cursor `38/2097190`, Center cursor `23/2097175`, `580`
  changed pixels, and `358176` red pixels. A follow-up run also records cursor
  artwork signatures for every checked normal/Alt cursor. Qt 6 and Qt 5 matched
  the signatures exactly: every cursor is `32x32@1.00` with hotspot `15,15`;
  Position/Center use hashes `1e19f313f559b6d3` and
  `b756652c7d59b6d3`, Rotation uses `137572f5ab7d9ccd` and
  `e5f471814b7d9ccd`, Scale uses `85c794af41f0f0e3` and
  `84d4229245f0f0e3`, and Shear uses `8161bce21e155637` and
  `812dd0e66155637`. The packaged Qt 5 run matched those measurements. This
  covers app-side Animate/Edit mode-specific cursor IDs, Alt precise-cursor
  decoration, cursor resource availability, and cursor artwork signatures
  through Qt mouse-move delivery; it does not validate real OS-level cursor
  delivery, unchecked cursor artwork variants, or full manual transform workflow
  visual parity.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-selection-tool-vector-handles-qt6`. This app-side
  smoke creates a vector stroke, switches the current tool to `T_Selection`,
  uses rectangular selection through `SceneViewer` Qt mouse events, hovers the
  selected vector stroke bbox scale handle, and drags that handle through Qt
  mouse events using the viewer DPR. It requires `selectionToolProbe=ok`,
  `selectionRectProbe=ok`, `selectionCursorProbe=ok`,
  `selectionHandleProbe=ok`, `mouseEventProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, one selected stroke, scale-cursor artwork, a
  changed selected-stroke bbox, changed framebuffer pixels, visible red content
  pixels, and before/after captures. The packaged Qt 6 run reported logical
  viewer size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, selection
  mode `Standard`, type `Rectangular`, selected stroke count `1`, bbox
  `-208.0000,-138.0000,208.0000,33.0000 ->
  -209.6763,-136.2378,279.8100,81.0663`, bbox delta `73.4863,46.3041`,
  cursor ID `38`, cursor artwork `ok`, `195080` changed pixels, and `174082`
  red pixels. The packaged Qt 5 run matched those measurements. This covers
  app-side vector Selection tool rectangular selection, scale-handle hover
  feedback, cursor resource availability, and scale-handle dragging through Qt
  mouse events. Separate smokes now cover advanced vector handles and vector
  Freehand/Polyline mode variants; real OS-level input/cursor delivery,
  raster selection workflows beyond rectangular/freehand/polyline app-side
  coverage, multi-object selection workflows, and full manual
  selection visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-selection-tool-vector-handle-variants-qt6`. This
  app-side smoke creates the same vector stroke, switches the current tool to
  `T_Selection`, selects the stroke with a rectangular drag, then hovers and
  drags the horizontal edge-scale handle, vertical edge-scale handle, and
  top-right rotation handle through `SceneViewer` Qt mouse events. It requires
  `selectionToolProbe=ok`, `selectionRectProbe=ok`,
  `selectionVariantCursorProbe=ok`, `selectionVariantHandleProbe=ok`,
  `mouseEventProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
  one selected stroke, cursor artwork for each checked handle, changed vector
  bbox measurements, changed framebuffer pixels, visible red content pixels,
  and before/after captures. The packaged Qt 6 run reported logical viewer
  size `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, selection mode
  `Standard`, type `Rectangular`, selected stroke count `1`, horizontal-scale
  bbox `-208.0000,-138.0000,208.0000,33.0000 ->
  -206.1030,-139.7834,260.7839,34.7834` with width delta `50.8869` and cursor
  `40`, vertical-scale bbox `-206.1030,-139.7834,260.7839,34.7834 ->
  -209.6923,-135.7615,264.3733,75.3727` with height delta `36.5673` and
  cursor `41`, rotation bbox `-209.6923,-135.7615,264.3733,75.3727 ->
  -188.4725,-179.3471,275.3424,77.2171` with cursor `33`, `198156` changed
  pixels, and `163929` red pixels. The packaged Qt 5 run matched those
  measurements. This covers app-side vector Selection tool rectangular
  selection, horizontal/vertical edge-scale handle cursor feedback, rotation
  handle cursor feedback, cursor resource availability, and handle dragging
  through Qt mouse events; real OS-level input/cursor delivery, other Selection
  tool modes, raster selection workflows beyond rectangular/freehand/polyline
  app-side coverage, multi-object selection workflows, and full manual
  selection visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-selection-tool-vector-center-thickness-deform-qt6`.
  This app-side smoke creates the same vector stroke, switches the current tool
  to `T_Selection`, selects the stroke with a rectangular drag, then hovers and
  drags the center handle, global thickness handle, and Ctrl free-deform
  top-right handle through `SceneViewer` Qt mouse events. It requires
  `selectionToolProbe=ok`, `selectionRectProbe=ok`,
  `selectionAdvancedCursorProbe=ok`, `selectionAdvancedHandleProbe=ok`,
  `mouseEventProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes,
  one selected stroke, cursor artwork for each checked handle, moved center
  coordinates, changed average stroke thickness, changed free-deform bbox
  measurements, changed framebuffer pixels, visible red content pixels, and
  before/after captures. The packaged Qt 6 run reported logical viewer size
  `994x819`, DPR `2` / `2.00`, framebuffer `1988x1638`, selection mode
  `Standard`, type `Rectangular`, selected stroke count `1`, center
  `0.0000,-52.5000 -> 44.6412,-86.8394` with cursor `31`, average thickness
  `22.6667 -> 31.5949` with cursor `32`, free-deform bbox
  `-216.9282,-146.9282,216.9282,41.9282 ->
  -216.1371,-152.5195,224.3624,29.0765` with cursor `20`, `66299` changed
  pixels, and `166236` red pixels. The packaged Qt 5 run matched those
  measurements. This covers app-side vector Selection tool center-handle,
  global-thickness, and Ctrl free-deform cursor feedback plus handle dragging
  through Qt mouse events; real OS-level input/cursor delivery,
  multi-object/multi-frame selection workflows, and full manual selection visual
  parity remain open.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-selection-tool-vector-mode-variants-qt6`. This
  app-side smoke creates two separated vector strokes, switches the current
  tool to `T_Selection`, sets the vector Selection type to `Freehand`, lasso
  selects the left stroke through `SceneViewer` Qt mouse events, clears the
  selection, then sets the type to `Polyline` and polygon-selects the right
  stroke through the same viewer event path. It requires
  `selectionToolProbe=ok`, `selectionFreehandProbe=ok`,
  `selectionPolylineProbe=ok`, `mouseEventProbe=ok`, `viewerRenderProbe=ok`,
  high-DPI framebuffer probes, two vector strokes, one selected stroke after
  each mode, stroke `0` selected only by Freehand, stroke `1` selected only by
  Polyline, a polyline bbox that differs from the freehand bbox, changed
  framebuffer pixels, visible red content pixels, and before/after captures.
  The packaged Qt 6 run reported logical viewer size `994x819`, DPR `2` /
  `2.00`, framebuffer `1988x1638`, selection mode `Standard`, freehand type
  `Freehand`, polyline type `Polyline`, vector stroke count `2`, freehand
  count `1`, freehand stroke booleans `true/false`, freehand bbox
  `-254.0000,-119.0000,-51.0000,29.0000`, polyline count `1`, polyline stroke
  booleans `false/true`, polyline bbox `41.0000,-109.0000,269.0000,39.0000`,
  `selectionBBoxChangedFromFreehand=true`, `4249` changed pixels, and
  `132404` red pixels. The packaged Qt 5 run matched the state and bbox
  measurements with `3989` changed pixels and `104339` red pixels. This covers
  app-side vector Selection tool Freehand and Polyline selection through Qt
  mouse events; real OS-level input/cursor delivery, multi-object/multi-frame
  selection workflows, and full manual selection visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-selection-tool-raster-handles-qt6`. This app-side
  smoke creates a full-color raster level, switches the current tool to
  `T_Selection`, selects a rectangular raster region through `SceneViewer` Qt
  mouse events, hovers the selected raster bbox scale handle, and drags that
  handle through Qt mouse events using the viewer DPR. It requires
  `selectionToolProbe=ok`, `selectionRasterProbe=ok`,
  `selectionCursorProbe=ok`, `selectionHandleProbe=ok`,
  `mouseEventProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes, a
  non-empty raster selection, scale-cursor artwork, a floating raster
  selection after the drag, a changed raster selection bbox, changed
  framebuffer pixels, visible red content pixels, and before/after captures.
  The packaged Qt 6 run reported logical viewer size `994x819`, DPR `2` /
  `2.00`, framebuffer `1988x1638`, selection mode `not-applicable`, type
  `Rectangular`, empty state `true -> false`, floating state `false -> true`,
  bbox `-90.0000,-85.0000,95.0000,90.0000 ->
  -90.0000,-85.0000,150.2195,135.1961`, bbox delta `55.2195,45.1961`, cursor
  ID `38`, cursor artwork `ok`, `39449` changed pixels, and `391090` red
  pixels. The packaged Qt 5 run matched those measurements except for `39448`
  changed pixels. This covers app-side raster Selection tool rectangular
  selection, scale-handle hover feedback, cursor resource availability, and
  scale-handle dragging through Qt mouse events; real OS-level input/cursor
  delivery, raster selection workflows beyond rectangular/freehand/polyline
  app-side coverage, multi-object and multi-frame selection workflows, and full
  manual selection visual parity remain open.
- On 2026-05-31, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-selection-tool-raster-mode-variants-qt6`. This
  app-side smoke creates a full-color raster level, switches the current tool
  to `T_Selection`, sets the raster Selection type to `Freehand`, replays a
  lasso path through `SceneViewer` Qt mouse events, then sets the type to
  `Polyline` and replays a click/double-click polygon through the same viewer
  event path. It requires `selectionToolProbe=ok`,
  `selectionFreehandProbe=ok`, `selectionPolylineProbe=ok`,
  `mouseEventProbe=ok`, `viewerRenderProbe=ok`, high-DPI framebuffer probes, a
  non-empty freehand raster selection, a non-empty polyline raster selection, a
  polyline bbox that differs from the freehand bbox, changed framebuffer pixels,
  visible red content pixels, and before/after captures. The packaged Qt 6 run
  reported logical viewer size `994x819`, DPR `2` / `2.00`, framebuffer
  `1988x1638`, selection mode `not-applicable`, freehand type `Freehand`,
  polyline type `Polyline`, freehand empty state `true -> false`, freehand
  floating state `false`, freehand bbox
  `-128.0000,-128.0000,-2.6106,74.1808`, polyline empty state `false`,
  polyline floating state `false`, polyline bbox
  `35.1826,-94.8777,128.0000,105.1491`,
  `selectionBBoxChangedFromFreehand=true`, `2974` changed pixels, and `356418`
  red pixels. The packaged Qt 5 run matched those state and bbox measurements
  with `2781` changed pixels and `356418` red pixels. This covers app-side
  raster Selection tool Freehand and Polyline selection through Qt mouse
  events; real OS-level input/cursor delivery, raster selection workflows beyond
  rectangular/freehand/polyline app-side coverage, multi-object and multi-frame
  selection workflows, and full manual selection visual parity remain open.
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
  mapping, timeline/UI onion workflow, tool overlays, manual FX Preview UI and
  broader FX preview workflows, broader final-render parity, or visual parity
  against the Qt 5 baseline.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-raster-brush-mouse-events-qt6`. This smoke sends
  Qt mouse press/move/release events through `SceneViewer`, maps the shared
  world-space brush path into logical widget coordinates using the viewer DPR,
  verifies increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. The same mouse-event smoke was
  also run against the Qt 5 build. This narrows Qt widget event dispatch and
  high-DPI input coordinate coverage, but it still does not validate real
  OS-level CGEvent/System Events delivery, tablet input, timeline/UI onion
  workflow, overlays, manual FX Preview UI and broader FX preview workflows,
  broader final-render parity, or visual parity.
- On 2026-05-30, the packaged Qt 6 app also passes
  `mise run gui-smoke-viewer-raster-brush-tablet-events-qt6`. This smoke sends
  synthetic Qt tablet press/move/release events through `SceneViewer`, maps the
  shared world-space brush path into logical widget coordinates using the
  viewer DPR, drives pressure from 0.35 to 0.85 with tilt `-18,12`, verifies
  increased raster opaque/red pixel counts, and requires changed plus
  red-dominant `SceneViewer` framebuffer pixels. The same synthetic tablet
  smoke was also run against the Qt 5 build. This narrows Qt tablet-event
  dispatch, `TMouseEvent` tablet/pressure propagation, and high-DPI input
  coordinate coverage. Real tablet hardware is not available for this migration
  pass, so hardware pressure/tilt should stay deferred unless a device or
  credible OS-level simulator is added; the synthetic smoke does not validate
  OS-level CGEvent/System Events delivery, platform driver pressure/tilt,
  timeline/UI onion workflow, overlays, manual FX Preview UI and broader FX
  preview workflows, broader final-render parity, or visual parity.
- On 2026-05-30, this branch added the permission-sensitive
  `mise run gui-smoke-viewer-raster-brush-system-events-qt6` gate for real
  macOS OS-level input delivery. The app-side hook now prepares the same raster
  brush scene, publishes screen coordinates, waits for external input, and
  requires raster opaque/red pixel counts plus `SceneViewer` framebuffer pixels
  to change. The harness now launches this permission-sensitive smoke through
  LaunchServices with explicit smoke environment/log routing, preflights
  `CGPreflightPostEventAccess()`, attempts AX raise/frontmost plus bundle/PID
  activation, reports the frontmost application, reports target window bounds
  and point containment, posts both the Qt global stroke and a diagnostic
  backing-scaled stroke, and posts a prime click before the drag. In the
  current local session, this gate is still not green: `cgPostEventAccess=true`
  and `axProcessTrusted=true`, the Qt global points are inside the OpenToonz
  window while backing-scaled points are outside it, but the frontmost
  application remains `com.apple.loginwindow`, the app-side event filter records
  `systemMouseAppEventCount=0` and `systemMouseViewerEventCount=0`, raster
  pixels stay at zero, and the System Events click fallback still fails with
  macOS error `-25200`. Treat this as a current desktop-session activation or
  OS-event delivery blocker, not as Qt 6 brush logic parity, because
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
  app targets, refreshed both macOS packages, added
  `viewer-onion-skin`, reran the Qt 6 arm64 bundle check, and verified that
  both packaged lanes pass with matching DPR, framebuffer, stage-player,
  current-frame red-pixel, and onion-pixel measurements.
  The follow-up row-area onion marker slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-onion-skin-rowarea`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, row-area high-DPI, row-area changed-pixel,
  MOS/FOS marker, toggle-state, onion-pixel, changed viewer-pixel, and
  red-pixel measurements. The follow-up row-area onion marker drag-range slice
  rebuilt both app targets, refreshed both macOS packages, added
  `viewer-onion-skin-rowarea-drag`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, row-area high-DPI, row-area changed-pixel, MOS range, onion-row,
  onion-pixel, changed viewer-pixel, and red-pixel measurements. The follow-up
  fixed onion marker drag-range slice rebuilt both app targets, refreshed both
  macOS packages, added `viewer-onion-skin-fixed-marker-drag`, reran the Qt 6
  and Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, row-area high-DPI, fixed-marker add/remove/front
  drag delivery, FOS add/remove/final counts, onion-row, onion-pixel, changed
  viewer-pixel, and red-pixel measurements. The follow-up
  onion-skin context-menu command slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-onion-skin-context-menu`, reran the Qt 6
  and Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, row-area high-DPI, row-area changed-pixel, onion
  command, MOS/FOS marker, onion-row, onion-pixel, changed viewer-pixel, and
  red-pixel measurements. The follow-up custom onion color slice rebuilt both
  app targets, refreshed both macOS packages, added
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
- The follow-up ruler-guide event slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-guide-events`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, ruler-widget, guide-count, guide-value,
  create/move/delete, changed-pixel, changed-neutral-guide-pixel, and red-pixel
  measurements.
- The follow-up ruler-guide variant slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-guide-variants`, reran the Qt 6 and
  Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, ruler-widget, horizontal and vertical drag-hide
  guide counts, hide endpoints, changed-neutral-guide-pixel, and red-pixel
  measurements.
- The follow-up ruler tick rendering slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-ticks`, reran the Qt 6 and Qt 5
  arm64 bundle checks, and verified that both packaged lanes pass with matching
  DPR, framebuffer, ruler-widget, ruler tick-pixel, changed tick-pixel, ruler
  unit, changed viewer-pixel, and red-pixel measurements.
- The follow-up ruler/guide style slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-ruler-guide-styles`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, ruler-widget high-DPI, custom ruler background,
  handle, and scale color-pixel, changed ruler-pixel,
  changed-neutral-guide-pixel, and red-pixel measurements.
- The follow-up ruler guide-line rendering slice rebuilt both app targets,
  refreshed both macOS packages, added `viewer-ruler-guide-lines`, reran the Qt
  6 and Qt 5 arm64 bundle checks, and verified that both packaged lanes pass
  with matching DPR, framebuffer, custom guide positions, dashed vertical and
  horizontal guide-line segments, expected guide-position bands, moved-guide
  changed-neutral-pixel, changed-pixel, and red-pixel measurements.
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
- The follow-up Animate tool active-axis drag slice rebuilt both app targets,
  refreshed both macOS packages, added `viewer-animate-tool-axis-drags`, reran
  the Qt 6 and Qt 5 arm64 bundle checks, and verified that both packaged lanes
  pass with matching DPR, framebuffer, Qt mouse-event dispatch, `T_Edit`/`Col1`
  state, Position X/Y movement, Rotation angle movement, Scale movement, Shear
  X/Y movement, Center movement, changed-pixel, and red-pixel measurements.
- The follow-up Animate tool cursor slice rebuilt both app targets, refreshed
  both macOS packages, added `viewer-animate-tool-cursors`, reran the Qt 6 and
  Qt 5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, Qt mouse-event dispatch, `T_Edit`/`Col1` state,
  Position/Rotation/Scale/Shear/Center cursor IDs, Alt precise cursor
  decoration, cursor pixmap availability, normal/Alt cursor artwork signatures,
  changed-pixel, and red-pixel measurements.
- The follow-up Selection tool vector handle slice rebuilt both app targets,
  refreshed both macOS packages, added
  `viewer-selection-tool-vector-handles`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, rectangular
  selection mode/type, selected stroke count, selected bbox scale-handle drag
  deltas, scale cursor ID/artwork availability, changed-pixel, and red-pixel
  measurements.
- The follow-up Selection tool vector handle-variant slice rebuilt both app
  targets, refreshed both macOS packages, added
  `viewer-selection-tool-vector-handle-variants`, reran the Qt 6 and Qt 5 arm64
  bundle checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, rectangular
  selection mode/type, selected stroke count, horizontal edge-scale delta,
  vertical edge-scale delta, rotation bbox delta, handle cursor IDs/artwork
  availability, changed-pixel, and red-pixel measurements.
- The follow-up Selection tool vector center/thickness/free-deform slice rebuilt
  both app targets, refreshed both macOS packages, added
  `viewer-selection-tool-vector-center-thickness-deform`, reran the Qt 6 and Qt
  5 arm64 bundle checks, and verified that both packaged lanes pass with
  matching DPR, framebuffer, Qt mouse-event dispatch, `T_Selection` state,
  rectangular selection mode/type, selected stroke count, center-handle delta,
  average-thickness delta, Ctrl free-deform bbox delta, handle cursor IDs/artwork
  availability, changed-pixel, and red-pixel measurements.
- The follow-up Selection tool vector mode slice rebuilt both app targets,
  refreshed both macOS packages, added
  `viewer-selection-tool-vector-mode-variants`, reran the Qt 6 and Qt 5 arm64
  bundle checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, Freehand and
  Polyline vector selection types, two separated vector strokes, Freehand
  selecting only stroke `0`, Polyline selecting only stroke `1`, distinct
  selected bboxes, changed-pixel, and red-pixel measurements.
- The follow-up Selection tool raster handle slice rebuilt both app targets,
  refreshed both macOS packages, added
  `viewer-selection-tool-raster-handles`, reran the Qt 6 and Qt 5 arm64 bundle
  checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, rectangular raster
  selection type, non-empty raster selection, floating-selection state after
  scale-handle drag, changed raster selection bbox, scale cursor ID/artwork
  availability, changed-pixel, and red-pixel measurements.
- The follow-up Selection tool raster mode slice rebuilt both app targets,
  refreshed both macOS packages, added
  `viewer-selection-tool-raster-mode-variants`, reran the Qt 6 and Qt 5 arm64
  bundle checks, and verified that both packaged lanes pass with matching DPR,
  framebuffer, Qt mouse-event dispatch, `T_Selection` state, Freehand and
  Polyline raster selection types, non-empty non-floating raster selections,
  distinct selected bboxes, changed-pixel, and red-pixel measurements.
- The follow-up Animate tool handle-variant slice rebuilt both app targets,
  refreshed both macOS packages, added `viewer-animate-tool-handle-variants`,
  reran the Qt 6 and Qt 5 arm64 bundle checks, and verified that both packaged
  lanes pass with matching DPR, framebuffer, Qt mouse-event dispatch,
  `T_Edit`/`Col1` state, All-mode fallback translation movement, ScaleXY X/Y
  movement, Shear X/Y movement, changed-pixel, and red-pixel measurements.
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
- A packaged Qt 6 audio-input backend smoke exists through
  `mise run gui-smoke-audio-input-qt6`. It starts the default Qt 6 audio input
  from inside the packaged app, records the selected input and format when
  available, captures a short buffer when the current session can deliver
  microphone data, and verifies `QAudioSource` reports `NoError`. The hook is
  bounded so microphone permission/device stalls report structured status
  instead of hanging. The current local aggregate run reported
  `audioInputProbe=timeout` and was treated as an environment skip. This does
  not prove packaged default-input backend capture, the Record Audio popup, WAV
  writing, permission UX, lip-sync timing, or noisy-room audio quality.
- A packaged Qt 6 audio-recording WAV smoke exists through
  `mise run gui-smoke-audio-recording-wav-qt6`. It records through the existing
  Record Audio WAV writer, saves a short WAV beside the GUI smoke status file,
  and reloads the result through `TSoundTrackReader` when microphone capture is
  available. The hook is bounded so microphone permission/device stalls report
  structured status instead of hanging. The current local aggregate run reported
  `audioRecordingWavProbe=timeout` and was treated as an environment skip. This
  does not prove the packaged Qt 6 capture-to-WAV writer path on this session,
  the Record Audio popup button flow, Save and Insert column insertion,
  microphone permission UX, lip-sync timing, or capture quality.
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
  `QCoreApplication` script mode. It also validates the legacy `dummy()`
  helper and checks that child scripts evaluated by `run()` can leave global
  variables and functions visible to the caller. The `run()` check uses
  `toonz/sources/tests/scriptengine/run_child.toonzscript`, copied by the
  smoke harness into the isolated `stuff/library/scripts` tree, so it exercises
  the same library-script lookup path as the legacy Qt 5 engine.
- `mise run check-qt6-script-scope` now guards the CMake boundary between the
  legacy Qt Script implementation and the Qt 6 `QJSEngine` facade. `Qt5::Script`
  component/target selection, legacy `scriptbinding_*` MOC headers, and legacy
  `scriptbinding_*` sources must remain inside `OPENTOONZ_QT_MAJOR EQUAL 5`
  blocks, Qt 6 must not gain a Qt Script target, and the orphaned app-side
  `toonz/scriptengine.cpp` debugger popup must not be added back to the
  OpenToonz app target because it depends on `QScriptEngineDebugger`.
- `mise run check-qt6-script-smoke-registry` now guards the Qt 6 script-smoke
  registry: every task listed in `scripts/qt6/run-all-script-smokes.sh` must
  exist in `mise.toml`, and every `OPENTOONZ_SCRIPT_FIXTURE` /
  `OPENTOONZ_SCRIPT_LIBRARY_FIXTURE` path referenced by the script-smoke tasks
  must exist on disk. It also fails if a new top-level script fixture is added
  without a corresponding `mise` task, except for the documented default
  `basic.toonzscript` / `run_child.toonzscript` pair.
- A `run()` error parity fixture exists at
  `toonz/sources/tests/scriptengine/run_errors.toonzscript` and is run by
  `mise run script-smoke-run-errors-qt6`. It validates that the Qt 6 bootstrap
  throws script-visible errors for missing arguments, extra arguments,
  non-file-path arguments, missing script files, and child-script exceptions
  from `toonz/sources/tests/scriptengine/run_error_child.toonzscript` instead
  of silently returning `undefined`.
- A second Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path.toonzscript` and is run by
  `mise run script-smoke-filepath-qt6`. It validates the first narrow
  `FilePath` compatibility facade on top of the Qt 6 `QJSEngine` backend.
- A FilePath coercion Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_coercion.toonzscript` and is run
  by `mise run script-smoke-filepath-coercion-qt6`. It validates
  `FilePath.valueOf()`, `String(filePath)`, JavaScript string concatenation,
  copy construction from another `FilePath`, and chained path mutation helpers.
- A FilePath mutation/metadata Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_mutation_metadata.toonzscript`
  and is run by `mise run script-smoke-filepath-mutation-metadata-qt6`. It
  validates mutable `extension`, `name`, and `parentDirectory` properties,
  `withParentDirectory()` string/FilePath conversion, `exists`, `isDirectory`,
  and `lastModified` exposure as a JS `Date`.
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
  errors, null cell-object errors, missing-level cell assignment errors,
  rejection of non-string, non-`Level` cell level arguments, four-argument
  undefined level rejection, bad row/column errors, and load-level
  missing-file/duplicate-name errors.
- A Scene argument edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_argument_edges.toonzscript` and is
  run by `mise run script-smoke-scene-argument-edges-qt6`. It validates
  strict Scene constructor arity, non-rendering Scene method arity checks for
  load/save, column insertion and deletion, cell access, level lookup/creation,
  and load-level calls. It also validates row/column argument rejection for
  insert/delete/get/set cell APIs, integer-only row/column index enforcement so
  fractional JavaScript numbers cannot be truncated at the native Qt boundary,
  backend negative row/column errors, bad frame-id rejection, and legacy
  frame-id-before-level-argument error precedence without entering scene icon,
  viewer, offscreen GL, or renderer paths.
- A Scene lifecycle edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_lifecycle_edges.toonzscript` and is
  run by `mise run script-smoke-scene-lifecycle-edges-qt6`. It validates
  cross-scene level isolation, scene disposal error behavior, and invalidated
  scene-owned level wrappers without entering viewer, offscreen GL, or renderer
  paths.
- A Scene level-wrapper Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_level_wrappers.toonzscript` and is
  run by `mise run script-smoke-scene-level-wrappers-qt6`. It validates
  `Scene.getLevels()` wrapper identity and lifetime behavior, including
  disposing a returned wrapper without deleting the scene-owned level and the
  current empty-wrapper behavior for stale scene-owned level wrappers after
  `Scene.dispose()`.
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
- A companion Scene load-level sequence Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_load_level_sequence.toonzscript` and
  is run by `mise run script-smoke-scene-loadlevel-sequence-qt6`. It validates
  multi-frame `Scene.loadLevel()` behavior, `Scene.getLevels()` /
  `Scene.getLevel()` registry lookup, name-based and object-based
  `Scene.setCell()` assignment, and a headless `Scene.save()` /
  `new Scene(path)` roundtrip for the loaded sequence.
- A Scene reload edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_reload_edges.toonzscript` and is run
  by `mise run script-smoke-scene-reload-edges-qt6`. It validates loading a
  saved scene into an existing `Scene` facade, replacing that scene with a
  second saved scene, restoring the first scene, checking the loaded level/cell
  state after each `Scene.load()`, and proving stale scene-owned `Level` wrappers
  are invalidated rather than kept live.
- A Scene failed-load Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_load_failure.toonzscript` and is run
  by `mise run script-smoke-scene-load-failure-qt6`. It validates that
  `Scene.load()` rejects an existing non-scene file before mutating the native
  scene, preserves current scene/cell data, and keeps existing scene-owned
  `Level` wrappers valid until a replacement scene successfully loads.
- A ninth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_builder.toonzscript` and is run by
  `mise run script-smoke-image-builder-qt6`. It validates the first narrow
  `Transform` and `ImageBuilder` compatibility slice: identity, translation,
  rotation, uniform scale, non-uniform scale, transform string reporting,
  translated raster composition, generated image access, image save and reload,
  `ImageBuilder.clear()` returning legacy `undefined`, clear/fill behavior,
  and typed Raster/ToonzRaster builder construction.
- An ImageBuilder edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_builder_edges.toonzscript` and is run
  by `mise run script-smoke-image-builder-edges-qt6`. It validates constructor
  argument-count/type/size errors, bad image type rejection, invalid fill color,
  empty-image add errors, non-`Transform` add argument rejection, ToonzRaster
  fill rejection, image type mismatch errors, strict ImageBuilder method arity
  for `clear()`, `fill()`, and `add()`, and disposed builder rejection for
  `toString()`, `image`, `clear()`, `fill()`, and `add()`.
- A Transform edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/transform_edges.toonzscript` and is run by
  `mise run script-smoke-transform-edges-qt6`. It validates finite-number
  argument rejection for `translate()`, `rotate()`, and `scale()`, strict
  constructor arity, plus disposed `toString()`, `translate()`, `rotate()`,
  and `scale()` rejection without entering rendering paths.
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
  images, empty Raster levels, and Vector levels, plus strict
  `ToonzRasterConverter` constructor arity and strict `foo()` helper arity.
- A converter lifecycle edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/toonz_raster_converter_lifecycle_edges.toonzscript`
  and is run by
  `mise run script-smoke-toonz-raster-converter-lifecycle-edges-qt6`. It
  validates instance `convert()` rejection after `dispose()`, disposed id
  reporting, stable `toString()` behavior, legacy bool coercion for the
  `flatSource` property, post-dispose `flatSource` persistence, and continued
  static `ToonzRasterConverter.convert()` behavior.
- An eleventh Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/outline_vectorizer.toonzscript` and is run
  by `mise run script-smoke-outline-vectorizer-qt6`. It validates the first
  narrow vectorizer binding slice: `OutlineVectorizer` construction, property
  get/set behavior, raster-image vectorization to a Vector `Image`, PLI
  save/reload, inserting the vectorized image into a vector `Level`, legacy
  `transparent` color-name acceptance, and invalid transparent-color rejection
  with the Qt Script-style color error.
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
  argument and type rejection for missing/extra arguments, non-image/non-level
  values, Vector images, empty Raster levels, and Vector levels, plus strict
  `OutlineVectorizer`/`CenterlineVectorizer` constructor arity, without
  entering renderer paths.
- A vectorizer property edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/vectorizer_property_edges.toonzscript` and
  is run by `mise run script-smoke-vectorizer-property-edges-qt6`. It validates
  `OutlineVectorizer` corner tuning property roundtrips, transparent-color
  parsing, numeric string coercion for `toneThreshold`,
  `CenterlineVectorizer` numeric property coercion, bool coercion for
  representative centerline toggles, invalid transparent-color rejection,
  strict constructor arity, and missing-property error behavior.
- A non-rendering binding lifecycle/property edge Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/binding_lifecycle_edges.toonzscript` and is
  run by `mise run script-smoke-binding-lifecycle-edges-qt6`. It validates
  property get/set roundtrips for `OutlineVectorizer`, `CenterlineVectorizer`,
  and `Rasterizer`, legacy bool coercion for representative bool properties,
  invalid `OutlineVectorizer.transparentColor` rejection, and
  disposed-object method/property rejection for `OutlineVectorizer`,
  `CenterlineVectorizer`, `Rasterizer`, and `ToonzRasterConverter`, including
  disposed `vectorize()`, `rasterize()`, converter instance `convert()`, and
  converter `foo()` calls.
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
  `Rasterizer.rasterize()` argument and type rejection for missing/extra
  arguments, non-image/non-level values, Raster/ToonzRaster images,
  Raster/Empty levels, strict `Rasterizer` constructor arity, and bad
  full-color resolution/DPI rejection while keeping the color-mapped path
  green.
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
- A Renderer frame/column selection Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/renderer_frames_columns.toonzscript` and
  is run by `mise run script-smoke-renderer-frames-columns-qt6`. It validates
  that `Renderer.frames` and `Renderer.columns` drive actual
  `renderScene()`/`renderFrame()` execution for a two-row, two-column Raster
  scene, including selected-column multi-frame output and default full-scene
  frame-list behavior.
- A Renderer vector-input Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/renderer_vector.toonzscript` and is run by
  `mise run script-smoke-renderer-vector-qt6`. It validates that a scene-owned
  Vector level can pass through `Renderer.renderFrame()` and
  `Renderer.renderScene()` under the Qt 6 `QJSEngine` facade, producing Raster
  image/level output that can be saved and reloaded.
- A Renderer edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/renderer_edges.toonzscript` and is run by
  `mise run script-smoke-renderer-edges-qt6`. It validates that
  `Renderer` rejects constructor arguments and that `renderScene()` and
  `renderFrame()` reject missing/extra/non-`Scene`/disposed scene arguments,
  bad non-integer frame values, extra `dumpCache()` arguments, and invalid
  `Renderer.frames` / `Renderer.columns` list values before reaching the Qt 6
  renderer path. It also validates integer-only frame/column selection lists so
  fractional JavaScript numbers cannot be truncated at the native Qt boundary,
  while preserving the legacy behavior that non-array `frames` / `columns`
  properties are treated as empty selection lists.
- A Renderer lifecycle edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/renderer_lifecycle_edges.toonzscript` and
  is run by `mise run script-smoke-renderer-lifecycle-edges-qt6`. It validates
  that `Renderer.toString()`, `renderScene()`, `renderFrame()`, and
  `dumpCache()` reject use after `Renderer.dispose()` while preserving the
  script-owned `frames` and `columns` arrays.
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
- A Level path edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_path_edges.toonzscript` and is run by
  `mise run script-smoke-level-path-edges-qt6`. It validates bad path setter
  rejection and the current failed path reload behavior, where a missing target
  path assignment leaves the wrapper as a zero-frame level at the requested
  path.
- A Level lifecycle edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_lifecycle_edges.toonzscript` and is
  run by `mise run script-smoke-level-lifecycle-edges-qt6`. It validates the
  current promoted-empty-level metadata before disposal, then verifies
  post-dispose rejection for string conversion, metadata/path access and
  mutation, frame access/mutation, `load()`, and `save()`.
- A sixteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_transformers.toonzscript` and is run
  by `mise run script-smoke-level-transformers-qt6`. It validates level-wide
  non-rendering transformer parity for `OutlineVectorizer.vectorize(Level)`,
  `CenterlineVectorizer.vectorize(Level)`, and `Rasterizer.rasterize(Level)`.
- A seventeenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_edges.toonzscript` and is run by
  `mise run script-smoke-filepath-edges-qt6`. It validates FilePath edge-case
  parity for relative concatenation, absolute-path concat rejection,
  non-directory `files()` errors, strict constructor and method arity for path
  mutation/listing helpers, and directory listing through the Qt 6 facade.
- A FilePath metadata Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_metadata.toonzscript` and is run
  by `mise run script-smoke-filepath-metadata-qt6`. It validates
  `exists`, `isDirectory`, directory listing, and `lastModified` as a JS
  `Date` object for files, directories, and missing paths.
- A companion Qt 6 path-argument fixture exists at
  `toonz/sources/tests/scriptengine/path_arguments.toonzscript` and is run by
  `mise run script-smoke-path-arguments-qt6`. It validates legacy-style
  string/FilePath argument checking for `run()`, FilePath parent/concat
  helpers, Image/Level/Scene path methods, and strict `FilePath`/`Level`/
  `Scene` constructor edge behavior that should not silently ignore extra
  arguments or coerce arbitrary values through `String()`.
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
  boundary check; under Qt's `offscreen` platform the Qt 6 scene-icon save path
  writes a valid background-filled icon instead of entering `TOfflineGL`, which
  avoids the unsupported offscreen OpenGL context path used only by automation.
  Normal GUI/platform sessions still use the existing rendered scene-icon path.
  This does not claim scene-icon visual parity, viewer parity, or broader
  render-output parity.
- A companion Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/scene_save_icon_variants.toonzscript` and
  is run by `mise run script-smoke-scene-save-icon-variants-qt6`. It validates
  the same `QApplication` scene-icon path for a two-column raster scene and a
  second save, then checks that both generated `sceneIcons` PNGs load through
  the Qt 6 `Image` facade with stable nonzero dimensions. This remains a
  scene-icon/offscreen boundary check; it does not claim pixel-level icon
  parity, rendered icon parity, or viewer parity.
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
  out-of-range, non-number, and fractional frame-index errors so fractional
  JavaScript numbers cannot be truncated at the native Qt boundary, bad frame-id
  rejection, empty-level name setter no-op parity, empty-level path `undefined`
  parity, legacy non-image `setFrame()` rejection, legacy
  frame-id-before-image error precedence, level/image type mismatch rejection,
  incompatible save rejection, missing-level load errors, and strict Level
  method arity for constructor, frame access, frame assignment, load, and save
  calls.
- An Image edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_edges.toonzscript` and is run by
  `mise run script-smoke-image-edges-qt6`. It validates empty image metadata
  and save errors, constructor argument-count errors, non-path argument
  rejection, missing-image load errors that clear the image handle, incompatible
  raster/ToonzRaster save rejection, unrecognized output type errors, and
  strict Image method arity for load and save calls.
- An Image lifecycle edge-case Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_lifecycle_edges.toonzscript` and is
  run by `mise run script-smoke-image-lifecycle-edges-qt6`. It validates that
  a loaded raster `Image` reports legacy metadata before disposal, then rejects
  post-dispose `toString`, metadata access, `load()`, and `save()` calls with
  `Invalid Image object` while exposing the disposed id as `-1`.
- An Image level-first-frame Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_level_first_frame.toonzscript` and is
  run by `mise run script-smoke-image-level-first-frame-qt6`. It validates
  `Image.load()`/`new Image(path)` on a saved two-frame Raster image sequence,
  including the legacy `Loaded first frame of 2` warning, implicit first-frame
  fallback, explicit frame-2 path loading, Raster type reporting, size, DPI,
  and string reporting.
- The script smoke can run in bounded mode or natural-exit mode. The basic,
  `run()` error, FilePath, FilePath edges, FilePath metadata, path-argument,
  Scene, Scene cells, Scene columns,
  Scene cell frame-id type, Scene edges, Scene argument edges,
  Scene lifecycle edges, Scene load-level, Scene load-level sequence,
  Scene save/reopen, Scene reload edge,
  Scene failed-load, Scene save-icon, Scene save-icon variants, Scene frame-id,
  Level, Level I/O, Level edge-case, Level I/O types, Image, Image edge-case,
  Image level-first-frame, ImageBuilder, ImageBuilder edge-case,
  Transform edge-case, ToonzRasterConverter,
  ToonzRasterConverter level conversion, ToonzRasterConverter edge-case,
  OutlineVectorizer,
  CenterlineVectorizer, vectorizer edge-case, vectorizer property edge-case,
  binding lifecycle edge,
  Rasterizer including full-color output, Renderer, Renderer frame/column
  selection, Renderer vector input, Renderer edge-case, Wrapper id, Rasterizer
  edge-case, Level path, and level transformer fixtures pass in both modes for
  the current Qt 6 app bundle.
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
- On 2026-06-04, the focused scene-icon fixtures passed under
  `QT_QPA_PLATFORM=offscreen` after packaging the Qt 6 app bundle:
  `mise run script-smoke-scene-save-icon-qt6` and
  `mise run script-smoke-scene-save-icon-variants-qt6`. The broader aggregate
  run with forced `QT_QPA_PLATFORM=offscreen` still fails at
  `script-smoke-rasterizer-qt6` when full-color Rasterizer output enters
  `TOfflineGL` and Qt reports that the offscreen platform does not support
  `createPlatformOpenGLContext`. `QtOfflineGL` now validates failed offscreen
  surface, context, current-context, and framebuffer creation, so this path
  exits quickly with `Rasterization failed` instead of timing out in GL setup.
  Treat full-color Rasterizer/Renderer offscreen OpenGL behavior as an open
  Qt 6 validation/implementation gap, not as closed by the scene-icon fallback.
- The Qt 6 Script Console now restores the legacy GUI-only `view()` helper for
  `Image` and `Level` values by injecting a console bridge on top of the
  `QJSEngine` facade. The flipbook UI path remains in the `toonz` app layer,
  headless script-smoke execution still does not expose `view()`, and
  `mise run gui-smoke-script-console-view-qt6` validates the GUI helper against
  generated `Image` and `Level` values, warning output delivery, a repeated
  command after the `view()` calls, `run()` of a child script from the isolated
  library scripts folder, child-script print output, child-script return
  values, child-script global variable persistence back to the console,
  expected error display for invalid `view()` usage, and one Up-arrow
  command-history recall through the real console widget. Manual console
  verification is still required for broader interactive command editing,
  `run()` error paths, FlipBook cleanup, repeated sessions, and real user
  scripts.
- The Qt 6 replacement path intentionally does not provide an embedded
  `QScriptEngineDebugger` equivalent. The active supported path is the
  `QJSEngine` Script Console and `Run Script...` execution path; any future
  debugger replacement should be designed as new Qt 6 UI/tooling instead of
  reintroducing the orphaned Qt Script debugger popup.
- On macOS, `scripts/qt6/run-gui-smoke.sh` now launches packaged Qt 6 GUI
  smokes through LaunchServices by default instead of directly executing the
  bundle binary. This matches normal `.app` startup more closely and avoids a
  sandbox-sensitive stall inside `QApplication` construction seen with direct
  executable launch. Use `OPENTOONZ_GUI_SMOKE_DIRECT_EXEC=1` only for focused
  direct-launch diagnostics.
- On macOS, `scripts/qt6/run-script-smoke.sh` uses the same LaunchServices
  strategy for `OPENTOONZ_SCRIPT_USE_QAPPLICATION=1` fixtures while preserving
  direct execution for fast headless script fixtures. It passes an explicit
  `OPENTOONZ_SCRIPT_WORKING_DIRECTORY` so the app restores the isolated smoke
  root before executing script bootstrap and fixture code. When the executable
  is linked to Nix-store Qt, the harness also points Qt plugin paths at the
  matching Nix plugin directory and temporarily hides bundled Qt frameworks and
  `qt.conf` during the smoke, preventing LaunchServices from loading both Nix
  and bundled Qt frameworks in the same process. The GUI and script smoke
  harnesses serialize that temporary bundle-runtime mutation through a
  build-local lock, so focused smokes cannot race while hiding/restoring
  `qt.conf` or bundled frameworks. With this path,
  `mise run script-smokes-qt6` and
  `mise run script-smokes-natural-exit-qt6` pass, including scene-icon,
  full-color Rasterizer, and Renderer QApplication fixtures.
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
  app-side onion-skin, row-area onion marker UI, mobile drag ranges, fixed
  marker add/remove/front drags, onion-skin context-menu commands, custom onion
  colors, row-area onion marker behavior across both current xsheet/timeline
  orientations, camera-box,
  safe-area/field-guide, safe-area presets/colors, safe-area custom files,
  field-guide settings,
  ruler/guide, ruler-guide event, ruler-guide variant, ruler guide-line
  rendering, ruler tick rendering, ruler/guide style,
  Animate/Edit tool overlay, direct transform-drag, Qt mouse-event transform,
  and undo-redo/modifier-key/handle hit-test/handle-variant/active-axis
  transform smokes, mode-cursor feedback plus cursor artwork signatures,
  Selection tool vector rectangular selection, scale-handle dragging, and
  edge-scale/rotation/center/thickness/free-deform handle variants,
  vector Selection Freehand/Polyline mode variants,
  raster Selection rectangular selection and scale-handle dragging,
  vector/full-color raster direct-tool brush smokes, Previewer/FX Preview
  render-cache and flipbook-control smokes, and a `SceneViewer` Qt mouse-event
  plus synthetic tablet-event raster brush smoke exist, but real
  OS-level/hardware tablet input, full drawing workflows, remaining timeline
  onion workflows beyond the current app-side row-area marker/toggle/drag,
  fixed-marker, Shift and Trace, context/color/orientation coverage, remaining
  selection workflows beyond vector rectangular/freehand/polyline mode coverage,
  vector scale/edge/rotation/center/thickness/free-deform handle paths, and
  raster rectangular/freehand/polyline app-side paths,
  remaining cursor artwork outside the checked Animate/Edit mode cursor set,
  real OS-level transform dragging, remaining manual ruler-guide variants not
  covered by app-side
  create/move/delete/drag-hide, guide-line visual variants beyond app-side
  custom-position dashed-line coverage, broader overlay and
  high-DPI visual behavior, manual Preview/FX Preview UI, manual preview-save
  popup/overwrite/warning dialogs and broader production preview scenes,
  broader rendering, non-macOS packaging, scripting object binding parity, and
  hardware camera/audio smoke remain open.

## Next Implementation Slice

Do not redo the first runway slice unless the current branch has been discarded.
The next slice should make the Qt 6 app useful enough to run and diagnose:

1. Continue packaged Qt 6 interactive GUI smoke beyond the now-green scripted
   startup/create/open/xsheet-state, raster/vector viewer-framebuffer, direct
   viewer zoom/pan transform, app-side onion-skin, row-area onion marker UI and
   mobile drag ranges, fixed marker add/remove/front drags, onion-skin
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
   vector/raster brush tool-input, Qt mouse-event, and synthetic Qt
   tablet-event paths. A real macOS CGEvent/System Events raster-brush gate
   now exists, but the current local run is blocked by app activation or OS
   input delivery:
   CGEvent post access and AX trust are present, and the Qt global stroke
   points are inside the target OpenToonz window, but the active desktop session
   still leaves `loginwindow` frontmost, the app-side event filter sees zero
   mouse events, System Events still reports `-25200`, and raster pixels remain
   unchanged. Resolve or rerun that permissioned path in an unlocked/frontmost
   user session before treating OS-level mouse delivery as covered. Keep
   hardware tablet pressure/tilt deferred rather than blocking the Qt 6 port
   unless a real device or credible OS-level simulator becomes available, then
   continue into
   remaining timeline onion workflows beyond the current app-side row-area
   marker/toggle/drag, fixed-marker, Shift and Trace,
   context/color/orientation smokes, remaining selection workflows beyond
   vector rectangular/freehand/polyline mode coverage, vector
   scale/edge/rotation/center/thickness/free-deform handle paths, and raster
   rectangular/freehand/polyline app-side paths, real OS-level cursor delivery,
   remaining cursor artwork outside the checked Animate/Edit mode cursor set,
   real OS-level transform dragging,
   manual Preview/FX Preview UI, manual preview-save popup/overwrite/warning
   dialogs, production FX-heavy preview scenes,
   remaining manual ruler-guide variants not covered by
   app-side create/move/delete/drag-hide, guide-line visual variants beyond
   app-side custom-position dashed-line coverage, broader high-DPI input and
   viewer/rendering behavior, and OpenGL fallback warning triage.
2. Keep the app-side GUI smoke status hook narrow and test-only. System Events
   remains useful as a fallback/manual diagnostic, but do not make Qt 6
   create/open validation depend on macOS accessibility window discovery unless
   that path becomes repeatable again.
3. Extend the script fixture set beyond the current `run()` error, `FilePath`,
   FilePath-edge, FilePath metadata, path-argument, `Scene`, Scene edge-case,
   `Level`, `Level.path`,
   Level edge-case, level-wide transformer, Level I/O types including
   ToonzRaster reload frame access, scene load-level sequence,
   scene data save/reopen, Scene reload edge, Scene failed-load wrapper
   preservation,
   Scene save-icon, Scene save-icon variants,
   Scene frame-id
   handling, Scene cell frame-id type, Scene argument edge cases,
   Scene lifecycle edge cases, `Image`, Image edge-case,
   Image level-first-frame,
   `ImageBuilder`, ImageBuilder edge-case, Transform edge-case,
   ToonzRasterConverter, ToonzRasterConverter level conversion,
   ToonzRasterConverter edge-case,
   OutlineVectorizer/CenterlineVectorizer/Rasterizer including full-color
   Rasterizer output, Renderer
   renderFrame/renderScene/dumpCache, Renderer frame/column selection,
   Renderer vector input, Renderer edge-case, Wrapper id compatibility, and
   binding lifecycle/property edge slices into the next
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
   Scene lifecycle edges, Scene load-level, Scene load-level sequence,
   Scene save/reopen,
   Scene reload edge, Scene failed-load,
   Scene save-icon, Scene save-icon variants, Scene frame-id,
   Level,
   Level edge-case, Level I/O, Level path,
   level transformer, Image,
   Image edge-case, Image level-first-frame, ImageBuilder,
   ImageBuilder edge-case, Transform edge-case,
   Level I/O types,
   ToonzRasterConverter, ToonzRasterConverter level conversion,
   ToonzRasterConverter edge-case, OutlineVectorizer, CenterlineVectorizer,
   vectorizer edge-case, vectorizer property edge-case, binding lifecycle edge, Rasterizer including
   full-color output, Rasterizer edge-case, Renderer including `dumpCache()`,
   Renderer frame/column selection, Renderer vector input, Renderer edge-case,
   and Wrapper
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
9. Harden the experimental Qt 6 binary packaging workflow into release-quality
   packaging lanes for macOS, Linux, and Windows.
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
mise run check-no-qregexp
mise run check-core5compat-scope
mise run check-qt6-multimedia-scope
mise run check-qt6-script-scope
mise run check-qt6-script-smoke-registry
mise run check-qt6-fontmetrics-scope
mise run check-qt6-fontdatabase-scope
mise run check-qt6-highdpi-attribute-scope
mise run check-qt6-touch-scope
mise run check-qt6-qvariant-scope
mise run check-qt6-qkeysequence-scope
mise run check-qt6-mediaplayer-scope
mise run check-qt6-desktopwidget-scope
mise run check-qt6-combobox-activated-scope
mise run check-qt6-checkbox-state-scope
mise run check-qt6-buttongroup-scope
mise run check-qt6-wheelevent-scope
mise run check-qt6-graphicssceneevent-scope
mise run check-qt6-qaction-scope
mise run check-qt6-qcolor-scope
mise run check-qt6-qimage-mirrored-scope
mise run check-qt6-qglformat-scope
mise run check-qt6-qgllegacy-scope
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
mise run script-smoke-filepath-mutation-metadata-qt6
mise run script-smoke-path-arguments-qt6
mise run script-smoke-scene-qt6
mise run script-smoke-scene-cells-qt6
mise run script-smoke-scene-columns-qt6
mise run script-smoke-scene-cell-fids-qt6
mise run script-smoke-scene-edges-qt6
mise run script-smoke-scene-argument-edges-qt6
mise run script-smoke-scene-lifecycle-edges-qt6
mise run script-smoke-scene-level-wrappers-qt6
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
mise run script-smoke-vectorizer-property-edges-qt6
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
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-mutation-metadata-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-path-arguments-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-cells-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-columns-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-cell-fids-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-argument-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-lifecycle-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-level-wrappers-qt6
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
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-vectorizer-property-edges-qt6
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
