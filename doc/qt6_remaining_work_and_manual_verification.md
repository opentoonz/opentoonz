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
- Direct `QRegExp` usage is guarded against by `mise run check-no-qregexp`,
  which now uses explicit C++ identifier-boundary matching so removed Qt 5 API
  tokens cannot slip past the grep check.
- Direct `QTextCodec` usage is isolated behind the legacy text-codec adapter
  and guarded by `mise run check-core5compat-scope`, using the same explicit
  identifier-boundary matching.
- `mise run check-no-qregexp` and `mise run check-core5compat-scope` now also
  run before the normal local configure, build, Qt 6 configure, and Qt 6
  translation-build tasks, so removed regex APIs and adapter-scope regressions
  fail before CMake/Nix work starts.
- Direct deprecated `QFontMetrics::width()` usage is isolated behind
  `QtCompat::fontMetricsHorizontalAdvance()` and guarded by
  `mise run check-qt6-fontmetrics-scope`.
- Direct deprecated `QFontDatabase` instance construction and
  `QTextCharFormat::setFontFamily()` usage are guarded by
  `mise run check-qt6-fontdatabase-scope`; use `QtCompat` font database and
  text-format helpers for those paths. The guard also verifies that the
  documented compatibility files only instantiate `QFontDatabase` inside Qt 5
  fallback branches.
- The lower-level `TFontManager` font family/style/private family/bold/italic
  queries no longer store a heap `QFontDatabase` instance. They use local
  Qt-version-aware helpers in `common/tvrender/tfont_qt.cpp` so this render
  layer avoids a `toonzqt` dependency while the Qt 6 lane uses static font
  database APIs.
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
- The macOS `TSystem::moveFileToRecycleBin()` path now resolves the user's
  `~/.Trash` with Qt instead of deprecated Carbon `FSFindFolder` /
  `FSRefMakePath` calls, while preserving the existing rename/copy/delete
  fallback behavior.
- Direct removed desktop-widget APIs are absent from `toonz/sources` and
  guarded by `mise run check-qt6-desktopwidget-scope`. Keep
  `QDesktopWidget`, `QApplication::desktop()`, `qApp->desktop()`, and direct
  generic `desktop()` calls out of feature code; use `QScreen` and
  widget-screen helpers instead. This is a source guard only. It does not
  replace manual multi-monitor, mixed-DPI, and off-primary-screen validation.
- An isolated Qt 6 translation lane exists through
  `nix-qt6-translation-check`, `mise run configure-qt6-translations`, and
  `mise run build-qt6-translations`. The build task keeps
  `WITH_TRANSLATION=ON` separate from the default Qt 5 and normal Qt 6 build
  directories, resolves Qt 6 `lrelease` through the imported `Qt6::lrelease`
  target, and verifies that the expected 63 generated `.qm` files are produced.
- Direct checkbox state-change connects in `toonz`, `toonzqt`, shared headers,
  stop-motion, and Type tool options are centralized behind
  `QtCompat::connectCheckStateChanged`, with a Qt 6.7+ `checkStateChanged` path
  and a Qt 5 `stateChanged` fallback. `mise run
  check-qt6-checkbox-state-scope` keeps direct `QCheckBox::stateChanged`
  connects out of feature code.
- Legacy `QButtonGroup` integer button-signal connects now use
  `QtCompat::connectButtonGroupIdClicked()` and
  `QtCompat::connectButtonGroupIdPressed()`. Both supported lanes use the non-deprecated `idClicked` /
  `idPressed` signals inside the helpers.
  `mise run check-qt6-buttongroup-scope` keeps direct legacy
  `buttonClicked(int)` / `buttonPressed(int)` connects out of feature code.
- Direct tablet-event `posF()` / `globalPos()` access and direct
  `QTabletEvent` construction are guarded by
  `mise run check-qt6-tabletevent-scope`, keeping those paths behind
  `QtCompat::tabletEventPositionF()`,
  `QtCompat::tabletEventGlobalPosition()`, and
  `QtCompat::makeTabletEvent()`. The Windows pointer-input bridge now uses
  the helper to preserve the existing synthetic pen/eraser, pressure, tilt,
  rotation, and button fields across the Qt 5 and Qt 6 constructor split. This
  does not replace manual hardware-tablet pressure and tilt validation.
- The active `QtOfflineGL` offscreen context path now uses `QSurfaceFormat`
  for shared Qt 5/Qt 6 format setup instead of an unused legacy `QGLFormat`
  block. `mise run check-qt6-qglformat-scope` keeps `QGLFormat` limited to the
  remaining Qt 5-only startup/PBuffer compatibility scope and verifies active
  references remain inside Qt 5-only preprocessor branches.
- `mise run check-qt6-qgllegacy-scope` keeps `QGLContext` and
  `QGLPixelBuffer` limited to the documented Qt 5-only OpenGL compatibility
  files and verifies active references remain inside Qt 5-only preprocessor
  branches.
- `QWheelEvent` pixel-delta access is centralized behind
  `QtCompat::wheelEventPixelDelta()` in current wheel handlers. `mise run
  check-qt6-wheelevent-scope` keeps direct `QWheelEvent::pixelDelta()` calls
  out of feature code.
- Schematic graphics-scene spin/aim handle drag deltas now use
  `QtCompat::graphicsSceneMouseEventScreenPosition()` and
  `QtCompat::graphicsSceneMouseEventLastScreenPosition()` instead of direct
  `screenPos()` / `lastScreenPos()` calls. `mise run
  check-qt6-graphicssceneevent-scope` guards that boundary.
- Common direct mouse/context/drop-event coordinate access is now guarded by
  `mise run check-qt6-mouseevent-scope`, keeping direct `x()`, `y()`, `pos()`,
  `globalPos()`, `globalX()`, and `globalY()` event accessors out of handler
  code after the `QtCompat` coordinate-helper migration.
- The recent flipbook image loader now avoids Qt 6-deprecated
  `QAction::parentWidget()` by casting `QAction::parent()`. `mise run
  check-qt6-qaction-scope` guards that QAction warning slice.
- Direct `QColor::setNamedColor()` usage is removed from feature code and
  guarded by `mise run check-qt6-qcolor-scope`; use `QColor(QString)` or a
  local color parsing helper for string color parsing.
- Direct feature-code `QImage::mirrored()` usage is centralized behind
  `QtCompat::mirroredImage()`. Qt 6 uses `QImage::flipped()` with explicit
  orientations, while Qt 5 keeps the existing `mirrored()` path inside the
  helper. `mise run check-qt6-qimage-mirrored-scope` keeps direct
  `QImage::mirrored()` calls out of feature code.
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
  flip-console slider/split-button controls, Function Sheet row and column-head
  context-menu placement, Function Tree channel middle-drag initiation,
  mini-toolbar dragging, and dock hover/drag/resize/separator/drop-placeholder
  paths, plus Color Field context-menu placement, Style Name Editor word-button
  context-menu placement, Palette Viewer tab-bar context-menu placement,
  Palette Viewer
  page-chip hit testing, drag thresholding, rename activation, tab dragging,
  tab rename activation, page-drop targeting, palette page context-menu
  placement, palette page tooltip placement, palette-icon drag thresholding, and
  save-toolbar drop hit testing, plus Studio Palette tree drag thresholding,
  row hit testing, context-menu placement, and drop-target tracking, now use
  `QtCompat` event-position helpers instead of direct Qt 5-era event coordinate
  accessors.
- Shared `toonzqt` Schematic Viewer panning, zooming, rubber-band gating, and
  double-click fit-scene hit testing, plus schematic port link-start geometry,
  group-editor title-bar hit testing, and FX/stage schematic node rename
  double-click hit testing, now use `QtCompat` event-position helpers instead
  of direct event coordinate calls.
- Shared `toonzqt` Spreadsheet panel panning, cell hit testing, and auto-pan
  edge tracking now use `QtCompat` mouse-position helpers instead of direct
  `QMouseEvent::pos()` calls.
- Shared `toonzqt` Style Editor color wheels, color sliders, parameter chips,
  style chooser chip hit testing, style chooser context-menu placement, and
  style chooser tooltip placement now use `QtCompat` event-position helpers
  instead of direct Qt 5-era event coordinate accessors.
- `toonz` Custom Panel scroller drag thresholds now use `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` Insert FX preset removal, Audio Recording popup, Filmstrip
  frame-panel, Pencil Test sub-camera preset, Command Bar, Xsheet Toolbar, Tool
  Properties panel, Command Bar customization tree, DvDirTreeView
  version-control tree, Menu Bar customization tree, room tabs, panel title-bar
  Safe Area/Preview menus, Viewer panel show/hide menu, Brush Preset panel
  show/hide menu, Layer Footer frames-per-page menu, Farm Server list, Camera
  Track preview, Palette Gizmo binding menu, Color Model viewer, task tree
  context-menu plumbing, `DvItemViewer` context-menu placement, and Xsheet PDF
  preview context-menu placement now use `QtCompat` context-menu helpers
  instead of direct Qt 5-era context-menu coordinate accessors.
- `toonz` room tab selection, reordering, and rename activation now use
  `QtCompat` mouse-position helpers instead of direct `QMouseEvent::pos()`
  calls.
- `toonz` History pane undo/redo row selection now uses `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` Camera Capture histogram range dragging now uses `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` Camera Track and Xsheet PDF export preview panning now use
  `QtCompat` mouse-position helpers instead of direct `QMouseEvent::pos()`
  calls.
- `toonz` Cleanup and Separate Colors swatch panning now use `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` Color Model picking now uses `QtCompat` mouse-position helpers
  instead of direct `QMouseEvent::pos()` calls.
- `toonz` Brush Preset item drag initiation and hotspot placement now use
  `QtCompat` mouse-position helpers instead of direct `QMouseEvent::pos()`
  calls.
- `toonz` Command Bar command-list hit testing now uses `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` Ruler guide creation, movement, deletion, and hover hit testing now
  use `QtCompat` mouse-position helpers instead of direct `QMouseEvent::pos()`
  calls.
- `toonz` Board Settings item hover and drag geometry now use `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` floating panel title-bar close-button hover and click hit testing now
  uses `QtCompat` mouse-position helpers instead of direct
  `QMouseEvent::pos()` calls.
- `toonz` Startup scene-list hover hit testing now uses `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- Shared `toonzqt` tree item hit testing, drag dispatch, context-menu
  placement, and tone-curve control-point editing now use `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` /
  `globalPos()` calls.
- `toonz` `DvItemViewer` item selection, play-button hit testing,
  middle-button panning, drag initiation, rename activation, table-column
  resize tracking, and item tooltip placement now use `QtCompat` event-position
  helpers instead of direct Qt 5-era local/global event coordinate calls.
- `toonz` Xsheet keyframe mover click/drag cell mapping now uses `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `toonz` Pencil Test sub-camera handle hit testing and drag resizing now use
  `QtCompat` mouse-position helpers instead of direct `QMouseEvent::pos()`
  calls in both active camera widget implementations.
- `toonz` Pencil Test capture image orientation flips now use
  `QImage::flipped()` on Qt 6 while preserving the existing `mirrored()` path
  on Qt 5.
- `toonz` Filmstrip frame double-click selection, press selection, navigator
  panning initiation, inbetween-button hit testing, drag/drop arming,
  drag-select, auto-pan edge tracking, inbetween tooltip hover checks,
  frame-head onion-skin toggles, frame-head current-frame movement, and
  shift-trace frame-head hover hit testing now use `QtCompat` mouse-position
  helpers instead of direct `QMouseEvent::pos()` / `globalPos()` calls.
- `toonz` Xsheet row-area current-frame, onion-skin, shift-trace,
  navigation-tag, play-range, panning, auto-pan, marker removal, context-menu
  placement, and double-click onion toggle paths now use `QtCompat`
  mouse-position and context-menu helpers instead of direct local/global event
  coordinate calls.
- `toonz` Xsheet column-area motion-path menu, object-change popup hover,
  column selection, toggle hit testing, panning, auto-pan, settings popup
  placement, double-click rename hit testing, synthetic context-menu release,
  and context-menu placement now use `QtCompat` mouse-position and context-menu
  helpers instead of direct local/global event coordinate calls.
- `toonz` Xsheet cell-area note hit testing, cell/level drag initiation,
  sound-level handle hit testing, panning, auto-pan, tooltip targeting,
  double-click rename targeting, and context-menu placement now use `QtCompat`
  mouse-position and context-menu helpers instead of direct local/global event
  coordinate calls.
- `toonz` Xsheet drag tools for generic cell mapping, selection drags, note
  movement, column selection/movement, sound volume editing, and drag/drop
  target mapping now use `QtCompat` mouse-position and drop-position helpers
  instead of direct Qt 5-era event coordinate calls.
- `toonz` Xsheet level-cell move click/drag mapping now uses `QtCompat`
  mouse-position helpers instead of direct `QMouseEvent::pos()` calls.
- `tnztools` Screen Picker screen-rectangle press, drag, and release geometry
  now uses `QtCompat` mouse-position helpers instead of direct
  `QMouseEvent::pos()` calls.
- `tnztools` measured-value field middle-drag editing now uses
  `QtCompat::mouseEventPosition()` instead of direct `QMouseEvent::x()` calls.
- `toonz` Image Viewer panning, color picking, context-menu placement, and
  loadbox/zoom drag setup now use `QtCompat` mouse-position and context-menu
  helpers instead of direct local/global event coordinate calls.
- `SceneViewer` mouse/tablet event initialization, tablet context-menu
  delivery, widget context-menu delivery, tablet hover-edge handling, and mouse
  double-click mapping, plus GUI smoke system-event mouse diagnostics and
  synthetic smoke mouse-event construction, now use `QtCompat`
  mouse/tablet/context-menu position helpers instead of direct Qt 5-era event
  coordinate accessors.
- `PlaneViewer` and `SwatchViewer` context-menu placement now use `QtCompat`
  context-menu helpers instead of direct global-position accessors.
- `FunctionPanel` and its graph drag tools now use `QtCompat` mouse-position
  helpers for graph hit testing, panning, zooming, keyframe dragging, handle
  dragging, rectangular selection, stretch operations, and context-menu
  placement.
- Wheel-event zoom centers for the plane, swatch, schematic, Function Panel,
  Cleanup swatch, Separate Colors swatch, Image Viewer, SceneViewer, and
  Xsheet frame zoom, plus the optional straight-skeleton debugger wheel zoom,
  now use `QtCompat` wheel-position helpers instead of direct Qt 5-era
  `QWheelEvent` position/delta accessors.
- `DvTextEdit` mini-toolbar font-size population and text-family formatting now
  use Qt 5/Qt 6-compatible APIs instead of Qt 6-deprecated `QFontDatabase`
  instance construction and `QTextCharFormat::setFontFamily()`.
- Font parameter style lookup and preview font construction now use `QtCompat`
  helpers so Qt 6 can use the static `QFontDatabase` APIs while Qt 5 keeps the
  instance-based path. `mise run check-qt6-fontdatabase-scope` keeps direct
  deprecated font APIs out of feature code.
- The lower-level `TFontManager` path in `common/tvrender/tfont_qt.cpp` now
  uses local Qt-version-aware font database helpers for family/style/private
  family/bold/italic queries, preserving the Qt 5 behavior without storing a
  deprecated heap `QFontDatabase` instance in Qt 6.
- `t32bitsrv` raster exchange now casts the raw destination pixel pointer to
  `void *` for its intentional byte-copy path, removing the
  `-Wnontrivial-memcall` warning seen during `tnzcore` rebuilds.
- macOS recycle-bin handling now uses Qt to resolve `~/.Trash` instead of
  deprecated Carbon `FSFindFolder` / `FSRefMakePath` lookup, keeping the
  existing rename/copy/delete fallback path.
- Stop-motion camera-option sizing and legacy Pencil Test camera-label sizing
  now use `QtCompat::fontMetricsHorizontalAdvance()`, removing direct
  `QFontMetrics::width()` calls from that slice while preserving Qt 5 behavior.
- Configure Shortcuts multi-key conflict checking now uses
  `QtCompat::keySequenceEntryToInt()` instead of open-coded Qt-version checks.
  Qt 6 uses `QKeyCombination::toCombined()` inside `QtCompat`, and Qt 5 keeps
  the existing integer key-sequence path. `mise run
  check-qt6-qkeysequence-scope` keeps direct `toCombined()` calls inside
  `QtCompat`.
- Audio Recording and Auto Lip Sync media preview source/state handling now use
  `QtCompat::setMediaPlayerSource()` and `QtCompat::mediaPlayerState()`, so Qt 6
  keeps `QMediaPlayer::setSource()` / `playbackState()` inside the shared
  helper while Qt 5 keeps `setMedia()` / `state()`. `mise run
  check-qt6-mediaplayer-scope` guards that boundary.
- Separate Colors color-string defaults and settings restoration parse stored
  strings through `QColor(QString)` instead of calling Qt 6-deprecated
  `QColor::setNamedColor()`. `mise run check-qt6-qcolor-scope` keeps direct
  deprecated calls out of `toonz/sources`.
- Qt Script/QJSEngine color argument parsing and OutlineVectorizer transparent
  color assignment also use `QColor(QString)` instead of
  `QColor::setNamedColor()`.
- Viewer touch gesture paths are centralized behind `QtCompat` helpers so the
  Qt 6 lane uses `QTouchEvent::points()` and `QEventPoint` positions while the
  Qt 5 lane keeps the existing `touchPoints()` / `QTouchEvent::TouchPoint`
  behavior. Touch device type checks also route through
  `QtCompat::touchDeviceType()`, keeping the Qt 6 `QInputDevice` and Qt 5
  `QTouchDevice` type boundary in one place. `mise run check-qt6-touch-scope`
  keeps direct `QTouchEvent::touchPoints()` and `device()->type()` access
  inside `QtCompat`.
- Deprecated `QVariant::canConvert(QVariant::...)` overloads are no longer
  present in the current app/UI/header tree. Fixed-type settings restore paths
  use templated `canConvert<T>()` checks, and dynamic preference type checks
  route through a version-aware helper that uses `QMetaType` on Qt 6 while
  preserving the Qt 5 `QMetaType::Type` path. `mise run
  check-qt6-qvariant-scope` keeps non-template `canConvert(...)` calls inside
  that helper.
- `mise run check-qt6-highdpi-attribute-scope` now guards that the Qt 5-only
  high-DPI startup attributes `AA_EnableHighDpiScaling` and
  `AA_UseHighDpiPixmaps` stay inside
  `QT_VERSION < QT_VERSION_CHECK(6, 0, 0)` blocks.
- User-activated real combo-box index handling now routes through
  `QtCompat::connectComboBoxActivatedIndex()` instead of direct
  `QComboBox::activated(int)` connections. The cleanup covers Pencil Test,
  Camera Track export, cleanup settings panes, project selectors, tool option
  combos, board settings, Xsheet PDF export, Filmstrip, level settings, Xsheet
  column filters, histogram/function segment widgets, file browser DPI policy,
  format settings, and navigation-tag color selection. Custom project-local
  `ToolOptionPopupButton::activated(int)` signals remain on their custom signal
  path. `mise run check-qt6-combobox-activated-scope` guards the deprecated
  real-combo signal boundary, including old macro `SIGNAL(activated(int))`
  combo connections.
- Legacy `toonzfarm/tfarm/tbaseserver.cpp` Windows socket diagnostic messages
  now use bounded `snprintf()` formatting instead of unbounded `wsprintf()`,
  and the non-Windows send failure message is initialized before throwing.

Still needed:

- Keep the Qt 5 build and Qt 6 build both passing after every shared source
  change.
  Latest local Qt 6 build evidence: `mise run build-qt6` passed on June 5,
  2026, with the expanded local preflight chain enabled: Windows MSVC ABI,
  QRegExp, Core5Compat scope, multimedia scope, script scope, font metrics
  scope, and high-DPI startup attribute scope. Apple builds now define
  `GL_SILENCE_DEPRECATION` at the CMake compile-definition boundary, so known
  legacy OpenGL/GLUT deprecation-warning noise should no longer obscure newer
  Qt 6 build diagnostics.
  Latest targeted app-target rebuild evidence after the combo-box activation,
  QImage mirrored/flipped, checkbox state-change, QWheelEvent pixel-delta, and
  QGLFormat scope slices: on June 5, 2026,
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel` and
  `cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz --parallel`
  both passed. The Qt 5 run became a broad rebuild after Ninja recovered its
  build log, but still linked successfully.
- Reduce and eventually remove `Core5Compat` once the legacy text-codec adapter
  no longer needs it.
- Keep direct `QTextCodec`, `QRegExp`, and `QRegExpValidator` out of feature
  code.
- Verify packaged translations at runtime before treating translation support
  as release-ready: switch each supported UI language in a packaged Qt 6 build,
  restart the app, and spot-check menus, dialogs, tooltips, and fallback text.
- Continue reducing the Qt 6 deprecation warning frontier, including
  remaining legacy event-coordinate accessors outside the completed
  SceneViewer, Function Panel, and shared-widget slices and the broader OpenGL
  warning field.
- Promote the new `qt-6-experimental` binary-build workflow into release-quality
  Qt 6 CI coverage once its platform artifacts and deployment behavior are
  validated.
- Make any remaining Qt 5 specific setup in documentation conditional or
  clearly marked as Qt 5 only.

### 3. Qt Script To QJSEngine Productization

Already covered:

- Qt 6 has a QJSEngine-based runtime shell for the app's script system.
- The basic script smoke now covers `print`, `warning`, the legacy `dummy()`
  helper, the legacy `void` result object, `run()` library lookup, `run()`
  return values, and child-script global variable/function persistence.
- `mise run check-qt6-script-scope` now guards the CMake boundary between the
  legacy Qt Script implementation and the Qt 6 `QJSEngine` facade, keeping
  `Qt5::Script` and legacy `scriptbinding_*` compilation inside Qt 5-only
  blocks.
- `mise run check-qt6-script-smoke-registry` now guards the script-smoke
  registry: every task listed in `scripts/qt6/run-all-script-smokes.sh` must
  exist in `mise.toml`, and every script fixture path referenced by the
  script-smoke tasks must exist on disk.
- The `run()` error smoke covers missing/extra arguments, bad path arguments,
  missing script files, and child-script exception propagation.
- The smoke suite covers the current facades for file/path, scene, level,
  image, image builder, rasterizer, vectorizers, renderer, wrapper id, binding
  lifecycle, and several edge cases.
- On June 5, 2026, `mise run script-smokes-qt6` passed against the current Qt 6
  app bundle, covering the aggregate FilePath, Scene, Level, Image,
  ImageBuilder, Transform, ToonzRasterConverter, vectorizer, Rasterizer,
  Renderer, lifecycle, and wrapper-id fixture set.
- On June 5, 2026, `mise run script-smokes-natural-exit-qt6` also passed
  across the same fixture set with `OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1`,
  including the QApplication-backed Rasterizer and Renderer fixtures.
- The Image smoke coverage now includes post-dispose rejection for metadata
  access, string conversion, `load()`, and `save()`, in addition to load/save,
  first-frame sequence loading, incompatible save error coverage, and strict
  Image method arity for load and save calls.
- The FilePath smoke coverage now includes mutable property setters,
  `withParentDirectory()` parent conversion, `exists`, `isDirectory`, and
  `lastModified` JS `Date` exposure, plus `valueOf()`, `String(filePath)`,
  copy construction, JavaScript string concatenation, strict constructor arity,
  and strict method arity for path mutation/listing helpers.
- The path-argument smoke coverage now also verifies strict
  `FilePath`/`Level`/`Scene` constructor arity alongside string/FilePath
  argument rejection for `run()`, path helpers, and Image/Level/Scene path
  methods.
- The Scene smoke coverage now includes `Scene.getLevels()` wrapper identity
  and lifetime behavior, including disposal of returned wrappers without
  deleting scene-owned levels and the current empty-wrapper behavior for stale
  scene-owned level wrappers after `Scene.dispose()`. It also covers null and
  malformed cell-object rejection in `Scene.setCell()`, strict Scene
  constructor arity, strict method arity for non-rendering Scene load/save,
  cell, level, and load-level APIs, plus legacy frame-id-before-level-argument
  error precedence.
- The Level smoke coverage now includes `Level.path` bad setter rejection, the
  current failed path reload behavior for missing target paths, and
  post-dispose rejection across the Level public surface. It also covers the
  legacy non-image `Level.setFrame()` error path and frame-id-before-image
  error precedence, plus strict Level method arity for constructor, frame
  access, frame assignment, load, and save calls.
- The ToonzRasterConverter smoke coverage now includes legacy bool coercion for
  `flatSource`, instance `dispose()` behavior, post-dispose `flatSource`
  persistence, static conversion after instance disposal, strict constructor
  arity, strict `foo()` helper arity, and post-dispose rejection for instance
  `convert()` and `foo()` calls.
- The vectorizer/rasterizer lifecycle smoke coverage now includes disposed
  `vectorize()` and `rasterize()` rejection, not only property and `toString()`
  rejection, and covers legacy bool coercion for representative
  OutlineVectorizer, CenterlineVectorizer, and Rasterizer properties.
- The Renderer smoke coverage now includes strict constructor arity,
  `renderScene()` / `renderFrame()` argument arity checks, strict
  `dumpCache()` arity, post-dispose error behavior for `toString()`,
  `renderScene()`, `renderFrame()`, and `dumpCache()`, plus legacy non-array
  `frames` / `columns` handling.
- The ImageBuilder smoke covers legacy `ImageBuilder.clear()` returning
  `undefined`, clear/fill behavior, strict Transform constructor arity, strict
  ImageBuilder method arity for `clear()`, `fill()`, and `add()`, and
  post-dispose rejection for `toString()`, `image`, `clear()`, `fill()`, and
  `add()`. It also confirms that the Qt 6 color path accepts the legacy
  `transparent` color name.
- The OutlineVectorizer smoke now covers `transparentColor = "transparent"`
  round-tripping and invalid transparent-color rejection with the legacy
  Qt Script-style color error. The vectorizer edge smoke also covers strict
  `OutlineVectorizer`/`CenterlineVectorizer` constructor arity and strict
  `vectorize()` method arity for missing/extra arguments.
- The vectorizer property edge smoke now covers `OutlineVectorizer` corner
  tuning property roundtrips, transparent-color parsing, numeric string
  coercion for `toneThreshold`, `CenterlineVectorizer` numeric and bool
  property coercion, invalid transparent-color rejection, constructor arity
  rejection, and missing-property error behavior.
- The Rasterizer edge smoke covers strict `Rasterizer` constructor arity and
  strict `rasterize()` method arity for missing/extra arguments.
- The Transform smoke covers post-dispose rejection for `toString()`,
  `translate()`, `rotate()`, and `scale()`, plus finite-number argument
  validation.
- The focused QApplication scene-icon script smokes now pass under
  `QT_QPA_PLATFORM=offscreen`; the Qt 6 offscreen path writes a valid
  background-filled icon instead of entering the unsupported offscreen
  `TOfflineGL` path, while normal GUI/platform sessions keep the existing
  rendered scene-icon path.
- The packaged Qt 6 Script Console smoke covers the GUI-only `view()` helper
  for generated `Image` and `Level` values, warning output delivery, a
  repeated command after the `view()` calls, `run()` of a child script from the
  isolated library scripts folder, child-script print output, child-script
  return values, child-script global variable persistence back to the console,
  expected error display for invalid `view()` usage, and Up-arrow
  command-history recall inside the real console widget.
- On macOS, the GUI smoke wrapper launches the packaged `.app` through
  LaunchServices by default because direct executable launch can stall inside
  `QApplication` construction in sandboxed automation contexts. Set
  `OPENTOONZ_GUI_SMOKE_DIRECT_EXEC=1` only when intentionally debugging the
  direct executable path.
- On macOS, QApplication-based script smokes also launch through
  LaunchServices by default and restore an explicit script smoke working
  directory before fixture execution. When the executable is linked to
  Nix-store Qt, the harness now uses matching Nix Qt plugins and temporarily
  hides bundled Qt frameworks/`qt.conf` during the smoke so LaunchServices does
  not mix two Qt runtimes. The aggregate Qt 6 script smoke suite now passes in
  bounded and natural-exit modes, including full-color Rasterizer and Renderer
  fixtures.
- Aggregate script smoke tasks exist in both bounded and natural-exit modes.

Still needed:

- Finish porting the remaining object binding groups that are still partial or
  Qt 5 oriented.
- Expand compatibility fixtures with real user scripts, not just synthetic
  coverage.
- Verify legacy script-visible behavior for object lifetime, invalid argument
  handling, property conversion, arrays, frame ids, paths, scene mutation, and
  renderer output.
- Resolve the remaining forced-offscreen OpenGL gap for full-color Rasterizer
  and Renderer fixtures. A normal macOS LaunchServices-started QApplication
  script smoke pass validates those fixtures with a real platform OpenGL
  context, but a forced
  `QT_QPA_PLATFORM=offscreen` aggregate run currently reaches
  `script-smoke-rasterizer-qt6`, completes the color-mapped checks, then exits
  with `Rasterization failed` when full-color Rasterizer output enters
  `TOfflineGL` and Qt reports that the offscreen platform cannot create a
  platform OpenGL context. `QtOfflineGL` now treats failed offscreen surface,
  context, current-context, and framebuffer creation as normal C++ failures, so
  this unsupported path is reported quickly instead of hanging in GL setup.
- Decide the replacement story for `QScriptEngineDebugger`, which has no direct
  Qt 6 equivalent.
- Confirm Script Console behavior manually, including broader interactive
  command editing, `run()` error paths from the GUI console, `view(Image)`,
  `view(Level)`, FlipBook cleanup, behavior across repeated console sessions,
  and real user scripts. The app-side smoke now covers `print`, `warning`, a
  `run()` happy path with child output/return/global persistence, expected
  `view()` error display, and one Up-arrow history recall path.
- Remove `Qt5::Script` from all Qt 6 production targets.
- Keep the Qt 5 script backend or retire it only after the compatibility suite
  proves that Qt 6 behavior is acceptable.

### 4. Qt Multimedia, Audio, Camera, And Stop-Motion

Already covered:

- Qt 6 API migration work exists for audio playback, audio recording, active
  pencil-test camera paths, and stop-motion camera enumeration.
- Stop-motion capture-sound playback now uses `QSoundEffect` in both Qt lanes,
  removing the old Qt 5-only `QSound` branch while preserving the camera snap
  sound feature.
- `mise run check-qt6-multimedia-scope` now guards the current multimedia API
  boundary: `QSound` must not reappear, and Qt 5 video-surface APIs must remain
  confined to the legacy 32-bit Qt 5 `penciltestpopup_qt.*` fallback. The
  `toonz` CMake source split now explicitly keeps that fallback out of every
  Qt 6 lane by selecting the modern stop-motion/Pencil Test sources whenever
  `OPENTOONZ_QT_MAJOR` is 6, even if an unusual non-64-bit Qt 6 configuration
  is attempted. The guard also runs before the normal local configure, build,
  translation-build, and check tasks so multimedia API regressions fail early,
  and uses explicit C++ identifier boundaries for the forbidden API tokens.
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
- If the legacy 32-bit `penciltestpopup_qt.*` fallback is ever brought into the
  Qt 6 release scope, replace its Qt 5 `QAbstractVideoSurface` /
  `QVideoSurfaceFormat` handling with a Qt 6 video-frame path such as
  `QVideoSink`. The active Qt 6 Pencil Test path currently avoids that surface
  API and still needs real live-camera workflow validation.

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
- Focused scene-icon script save checks under Qt's offscreen platform.

Still needed:

- Manual File > Preview workflow validation.
- Manual schematic FX Preview command validation.
- Live FX Preview playback and timer behavior.
- Live sub-camera dragging and visual framing.
- Manual preview-save popup, overwrite, warning, and cancellation behavior.
- Production FX-heavy preview scenes.
- Broader final-render parity against Qt 5 using real scenes.
- Broader vector rendering parity.
- Full-color Rasterizer/Renderer validation under a real OpenGL-capable
  QApplication session, or a product-quality non-OpenGL fallback where
  appropriate.
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
- Packaged macOS Qt 6 app-side high-DPI and Script Console GUI smokes pass when
  run through the LaunchServices wrapper outside the sandbox.
- The bounded Qt 6 script-engine smoke suite, including the QApplication-backed
  scene icon, rasterizer, and renderer fixtures, passes when the app bundle is
  launched outside the sandbox.
- The full app-side `gui-smokes-app-qt6` aggregate passes outside the sandbox.
  The current run covered startup, scene creation, high-DPI, Script Console,
  multimedia device enumeration, camera-format no-camera handling, audio
  playback/output, xsheet scrub, raster/vector viewer framebuffers, preview/FX
  preview/final render outputs, onion skin, safe-area/field-guide/ruler-guide
  overlays, Animate/Edit cursor and drag paths, vector/raster Selection modes,
  vector brush, raster brush, Qt mouse-event raster brush delivery, and
  synthetic Qt tablet-event raster brush delivery.
- On the current machine/session, the audio-input and audio-recording WAV
  smokes report structured `timeout` skips rather than failures. Treat those as
  environment-limited microphone capture coverage, not proof of microphone
  recording parity.
- The `qt-6-experimental` GitHub Actions workflow builds Qt 6 Linux, Windows,
  and macOS artifacts from `codex/qt-6-port` on `bgyss/opentoonz`, runs weekly
  on Mondays at 10:00 UTC or by manual dispatch, and publishes them to the
  rolling `qt-6-experimental` prerelease. `mise run release-qt6-experimental`
  retags the current clean branch and pushes the tag to refresh that rolling
  prerelease manually.

macOS still needed:

- Re-run packaging after every GUI-affecting relink before manual testing.
- Reduce repeated `macdeployqt` install-name churn if it keeps slowing the
  loop.
- Re-check release signing, notarization, helper executables, and copied
  runtime data.
- Confirm packaged Qt plugins and frameworks are loaded from the bundle.

Windows still needed:

- Keep `mise run check-windows-msvc-abi` passing before local Qt 5/Qt 6 builds;
  it is an early warning for MSVC DLL import/export annotation mistakes, not a
  replacement for the real Windows workflow.
- Validate the new `qt-6-experimental` Windows x64 binary workflow on GitHub
  Actions.
- Re-audit the Qt 6 `aqtinstall` module list, `windeployqt` arguments, and
  plugin output.
- Validate OpenGL, tablet/stylus, camera, audio, and file associations.

Linux still needed:

- Validate the new `qt-6-experimental` Linux x86_64 AppImage workflow on GitHub
  Actions.
- Audit the Qt 6 `aqtinstall` module list, AppImage bundling, and plugin
  coverage.
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
mise run gui-smokes-app-qt6
```

The aggregate above runs the packaged GUI smokes that do not require real
OS-level input delivery. Audio input and audio-recording may pass as structured
`timeout` or `no-device` skips when the current session cannot capture from a
microphone; follow up with manual microphone recording before claiming audio
input parity. To bisect failures or run only one area, use the individual tasks:

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
mise run gui-smokes-app-qt6
```

For focused reruns:

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
