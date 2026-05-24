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
2. Finish the active Metal migration to a mergeable checkpoint before doing
   broad rendering-side Qt 6 rewrites.
3. Start Qt 6 work now only where it is independent of Metal: CMake scaffolding,
   deprecated Qt 5.15 API cleanup, `QDesktopWidget`, `QRegExp`, `QTextCodec`,
   Qt Script replacement design, Nix/mise dependency planning, and CI matrix
   design.
4. Do not start a large Qt 6 rendering branch from the old OpenGL state if the
   Metal worktree is already changing viewer, renderer, or offscreen GL
   boundaries.
5. Treat `Core5Compat` as a temporary bridge only. It can keep `QTextCodec` or
   `QRegExp` compiling during early bring-up, but the release-quality port
   should move to maintained Qt 6 APIs wherever practical.

In practical sequencing terms: do not pause the Metal migration. Pause only the
parts of the Qt 6 migration that would rewrite the same rendering files. Finish
Metal to a stable integration point, then make Qt 6 consume that boundary.

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
  still uses Qt-specific CMake commands when enabled.

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
- `Core5Compat` is a temporary bridge for removed Qt 5 Core APIs such as
  `QTextCodec` and `QRegExp`.
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
| Regular expressions | 8 files reference `QRegExp` or `QRegExpValidator` | Port to `QRegularExpression` and `QRegularExpressionValidator`; avoid long-term `Core5Compat` dependency |
| Text codecs | 7 files reference `QTextCodec` or text-stream codec APIs | Either bridge with `Core5Compat` or create explicit legacy encoding adapters |
| Qt OpenGL classes | 34 files mention `QOpenGL*`, `QGLWidget`, `QSurfaceFormat`, or Qt offscreen GL classes | Needs Qt 6 module changes and coordination with the Metal migration |
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
- The Qt 5 default lane currently compiles and links the `OpenToonz` app target
  with `cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz
  --parallel`.
- The Qt 6 lane currently compiles and links the `OpenToonz` app target with
  `cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
  --parallel`. This is a compile/link milestone only, not product-ready Qt 6
  support.
- The Qt 6 macOS app bundle now packages and passes the arm64 bundle check with
  `mise run package-macos-qt6` and `mise run check-macos-arm64-qt6`.
- First removed-API fixes in this branch include `QDesktopWidget` to `QScreen`
  helper usage, `QMutex::Recursive` to `QRecursiveMutex`, Qt 6 audio playback
  construction, `QMatrix` to `QTransform` in `iwa_floorbumpfx.cpp`, and local
  `qsizetype` size fixes in `iwa_particlesengine.cpp`.
- Later compile-frontier fixes in this branch include removal of stale
  `QDirModel` usage, removal of remaining `QRegExp` hits, Qt 6-safe layout
  margin calls, Qt 6-safe MOC guards using `OPENTOONZ_QT_MAJOR`, a stop-motion
  camera-info helper around `QMediaDevices`/`QCameraDevice`, and removal of a
  stale `FileInfoPopup` MOC entry that generated an invalid Qt 6 metatype
  reference.
- Qt 6 builds still emit many macOS OpenGL deprecation warnings. Those are not
  treated as this slice's migration failures because broad GL/viewer churn
  should wait for the Metal checkpoint.

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
- Replace remaining `QRegExp` and `QRegExpValidator` with
  `QRegularExpression` and `QRegularExpressionValidator`.
- Replace `QGLWidget::convertToGLFormat` uses with a local image conversion
  helper. The current direct uses are in `tnztools/toolutils.cpp` and
  `tnztools/skeletontool.cpp`.
- Audit `QTextCodec` call sites and decide which legacy encodings must remain
  exact. The visible cases include Shift-JIS, GBK, UTF-8, PSD layer names, sound
  columns, simple levels, and SXF import/export.
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
  Qt 6 script fixture. `mise run script-smoke-qt6` runs it through the packaged
  Qt 6 app and verifies `print`, `warning`, and `run` output.
- The script fixture can run in bounded mode or natural-exit mode. Both
  `mise run script-smoke-qt6` and
  `OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6` pass for the
  current packaged Qt 6 app.
- The OpenToonz object binding groups are still Qt 5-only in this branch:
  files, scenes, levels, images, renderer, rasterizer, vectorizers, and
  converters. This is an explicit temporary limitation, not product-ready Qt 6
  script support.
- The script console warns in Qt 6 that the OpenToonz object bindings have not
  been ported yet.

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
- Real camera and audio hardware smoke tests have not been completed. Treat
  the media work as compile-ready, not behavior-certified.

### 5. OpenGL, Viewer, Offscreen Rendering, And Metal

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

Because a Metal migration is already happening in another worktree, the Qt 6
rendering work should not start as a parallel rewrite of the same old OpenGL
files. The safer plan is:

1. Let the Metal migration establish a renderer/backend boundary.
2. Merge or rebase Qt 6 work on top of that boundary.
3. Keep a compatibility OpenGL backend only where necessary for non-macOS or
   transitional validation.
4. Make Qt 6 deal with widget integration, event handling, high-DPI, and
   packaging, not with inventing a second renderer architecture.

If the Metal work is long-running, the Qt 6 branch can still do non-rendering
cleanup now. It should avoid churn in files like:

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
- The compile work deliberately avoided broad viewer, renderer, offscreen GL,
  shader, and tool-overlay rewrites. The remaining OpenGL and high-DPI warning
  cleanup should be scheduled after the Metal checkpoint or against an explicit
  renderer/backend boundary.
- An offscreen Qt 6 startup smoke is not a useful GUI substitute on macOS
  because the offscreen platform cannot create the OpenGL context OpenToonz
  still expects.
- A bounded Cocoa startup smoke with a copied `stuff` tree stayed alive until
  the smoke harness stopped it. That is a better first startup signal than the
  offscreen run, but it still needs an interactive GUI smoke before runtime
  behavior is considered validated.

### 6. Platform Packaging

Packaging has to be treated as part of the port.

macOS:

- Add a Qt 6 package lane rather than mutating the Qt 5 package script in place.
- Re-audit plugin group names and deployed library names after `macdeployqt`.
- Re-check app bundle structure, ad hoc signing, release signing, notarization,
  helper executables, and copied `stuff`.
- Verify whether the Metal migration changes bundled framework or entitlement
  requirements.

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
- `mise` now has `package-macos-qt6` and `check-macos-arm64-qt6` tasks that
  point the existing macOS package and architecture-check scripts at the Qt 6
  app bundle.
- `mise run package-macos-qt6` now completes and ad hoc signs the Qt 6 bundle.
  `mise run check-macos-arm64-qt6` reports the main binary as arm64 and checks
  291 Mach-O files for arm64.
- The first Qt 6 Cocoa startup smoke reported missing multimedia backends and
  SVG pixmap load failures. After the plugin-path fix, a second bounded Cocoa
  smoke from the Qt 6 Nix shell loaded the Qt Multimedia FFmpeg backend and no
  longer reported SVG pixmap failures. It still needs an interactive GUI smoke;
  the bounded harness stops the app after startup and therefore cannot validate
  user workflows.
- A bounded startup smoke of the packaged Qt 6 app now gets past the prior
  duplicate-Qt/platform-plugin abort and loads the Qt Multimedia FFmpeg backend
  from the package. The smoke still reports OpenGL fallback messages and
  crash-handler `atos` noise after the harness terminates the process, so this
  is not a substitute for an interactive GUI smoke.

Windows:

- The current workflow depends on a custom Qt 5.15.2 with WinTab support.
  Existing repo docs say this custom build carries a WinTab feature that was
  introduced officially in Qt 6. A Qt 6 Windows lane should verify whether the
  custom Qt package can be removed.
- Replace the manual Qt 5 path setup with a reproducible Qt 6 acquisition
  strategy.
- Re-run tablet, stylus, OpenGL or Metal-equivalent rendering, camera, and
  audio checks.
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

### Phase 0: Freeze The Baseline And Metal Boundary

Goal: prevent two long-running branches from rewriting the same rendering
surface.

Tasks:

- Confirm the Qt 5 default build still configures with `mise run configure`.
- Confirm the Metal worktree's intended merge boundary and touched files.
- Decide whether Metal lands before Qt 6 rendering work. The recommendation is
  yes.
- Create a Qt 6 tracking document or issue with ownership by subsystem.
- Capture the current Qt 5 module list and all Qt 5-specific CMake references.

Exit criteria:

- Qt 5 default path remains unchanged.
- There is a written list of files the Qt 6 prep branch should avoid until the
  Metal work is stable.

### Phase 1: Qt 5.15-Compatible Cleanup

Goal: reduce known Qt 6 compile failures without changing the Qt major.

Tasks:

- Add a diagnostic `QT_DISABLE_DEPRECATED_UP_TO=0x050F00` lane.
- Run Clazy Qt 6 checks against the Qt 5 build.
- Port `QDesktopWidget` and `qApp->desktop()` to a local screen helper.
- Port `QRegExp` and `QRegExpValidator`.
- Replace direct `QGLWidget::convertToGLFormat` calls.
- Add a text-codec adapter for the existing `QTextCodec` call sites, even if
  the first implementation delegates to Qt 5.

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
- Replace `QAbstractVideoSurface` with `QVideoSink`-based handling.
- Add manual smoke scripts or repo-local checklists for camera and audio.

Exit criteria:

- App compiles with Qt 6 multimedia.
- Camera and audio smoke checks are documented and at least one local platform
  has been verified.

### Phase 5: Rendering Integration After Metal

Goal: make Qt 6 use the stable renderer/backend boundary.

Tasks:

- Rebase on the Metal checkpoint.
- Update viewer widgets for Qt 6 event, high-DPI, and OpenGLWidgets behavior.
- Keep direct GL compatibility only where required by non-macOS paths.
- Validate frame rendering, viewer drawing, onion skin, selection overlays,
  tool cursors, FX preview, and flipbook behavior.

Exit criteria:

- Qt 6 build runs the app on at least one desktop platform.
- A GUI smoke pass exercises viewer and drawing behavior.
- Known Metal-vs-OpenGL parity differences are documented.

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
  encoding requirement.
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
| `QDesktopWidget`, `QRegExp`, small deprecations | Medium | Low to medium |
| `QTextCodec` and legacy encodings | Medium | Medium |
| Qt Script replacement | Large | High |
| Qt Multimedia camera/audio | Large | High |
| OpenGL/Metal/Qt 6 viewer integration | Large | High |
| macOS packaging | Medium | Medium |
| Windows packaging and tablet validation | Large | High |
| Linux packaging | Medium | Medium |
| QA and release stabilization | Large | High |

The most important schedule decision is to avoid doing Qt 6 rendering work
against an obsolete OpenGL boundary while the Metal work is still changing that
same boundary. Let Metal reduce the rendering ambiguity, then port the Qt shell
and platform integration around it.

## Recommended Next Implementation Slice

The first infrastructure slice has now been achieved in this branch: the dual
Qt lane exists and both Qt 5 and Qt 6 can compile/link the `OpenToonz` app
target. The next implementation branch should not repeat that runway work. It
should turn the compile/link milestone into a runtime and subsystem validation
milestone:

1. Launch the Qt 6 build-tree app and document the first runtime blockers.
2. Verify that app resources, `stuff`, plugins, helper executables, and runtime
   paths are sufficient outside the linker step.
3. Extend the current script compatibility fixture set and use it to drive the
   next `QJSEngine` object-binding slice.
4. Run focused audio playback/recording and camera enumeration smoke tests on
   real hardware.
5. Harden the existing macOS Qt 6 packaging lane only when runtime evidence
   requires it; otherwise move package follow-up to Linux and Windows.
6. Keep viewer/rendering changes narrow until the Metal boundary is stable.

That slice gives later agents runtime evidence instead of another compile-only
milestone, while still avoiding overlap with the Metal branch.

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

Packaging checks once artifacts exist:

```sh
mise run package-macos-qt6
mise run check-macos-arm64-qt6
mise run script-smoke-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6
```

Manual smoke checks:

- launch from the build tree
- launch from packaged artifact
- open a scene
- draw raster and vector strokes
- scrub xsheet/timeline
- use onion skin
- render preview and final frame
- run a script fixture
- record and play audio
- use camera preview and still capture
- validate tablet/stylus input on Windows

## Final Answer On Metal Sequencing

Do not pause the Metal migration. Finish it to a stable checkpoint. Begin Qt 6
prep in parallel only where it is clearly independent of rendering. Once Metal
has landed or has a stable integration branch, start the Qt 6 rendering work
from that base.
