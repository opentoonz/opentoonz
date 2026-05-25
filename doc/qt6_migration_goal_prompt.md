# Codex Goal Prompt: OpenToonz Qt 6 Migration

Use this prompt to start a long-running Codex implementation session in this
checkout.

## Goal Prompt

You are working in the OpenToonz repository root.

Your objective is to turn the Qt 6 migration study into an incremental,
verified migration path. Preserve the current Qt 5 build while adding a Qt 6
lane. Do not do a big-bang port. Do not start broad rendering rewrites until the
active Metal migration has reached a stable merge point or an explicitly
documented renderer/backend boundary.

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

Do not pause the Metal migration. Let it finish to a mergeable checkpoint.

In this Qt 6 branch, avoid broad edits to viewer, renderer, offscreen GL,
shader, and tool overlay rendering files unless they are required for a narrow
compile fix and do not conflict with the Metal branch. If the live repository
shows that Metal already changed the rendering boundary, adapt to that boundary
instead of recreating an older OpenGL design.

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

As of May 25, 2026, the initial Qt 6 runway is already implemented in this
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
- The latest post-relink validation reran the macOS Qt 6 package, arm64 bundle
  check, bounded startup smoke, scene-create smoke, and generated-scene reopen
  smoke successfully against the current packaged app.
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
- A first Qt 6 script smoke fixture exists at
  `toonz/sources/tests/scriptengine/basic.toonzscript` and is run by
  `mise run script-smoke-qt6`. It validates the `QJSEngine` bootstrap for
  `print`, `warning`, and `run` through the Qt 6 app bundle in headless
  `QCoreApplication` script mode.
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
- A ninth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/image_builder.toonzscript` and is run by
  `mise run script-smoke-image-builder-qt6`. It validates the first narrow
  `Transform` and `ImageBuilder` compatibility slice: transform string
  reporting, translated raster composition, generated image access, image save
  and reload, clear/fill behavior, and typed raster builder construction.
- A tenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/toonz_raster_converter.toonzscript` and is
  run by `mise run script-smoke-toonz-raster-converter-qt6`. It validates the
  first narrow converter binding slice: `ToonzRasterConverter` construction,
  static raster-image conversion to a ToonzRaster `Image`, TLV save/reload, and
  existing compatibility helpers such as `toString()`, `foo()`, and
  `flatSource`.
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
- A thirteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/rasterizer.toonzscript` and is run by
  `mise run script-smoke-rasterizer-qt6`. It validates the first narrow
  Rasterizer binding slice: construction, property get/set behavior, and
  color-mapped vector-image rasterization to a ToonzRaster `Image`, plus TLV
  save/reload and insertion into a ToonzRaster `Level`. The full-color
  Rasterizer path remains deferred because it uses `TOfflineGL`.
- A fourteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/wrapper_id.toonzscript` and is run by
  `mise run script-smoke-wrapper-id-qt6`. It validates inherited Wrapper `id`
  parity for the non-rendering `QJSEngine` facades: `FilePath`, `Scene`,
  `Level`, `Image`, `Transform`, `ImageBuilder`, `ToonzRasterConverter`,
  `OutlineVectorizer`, `CenterlineVectorizer`, and `Rasterizer`, including
  disposal-time `-1` ids for disposable facade objects.
- A fifteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_path.toonzscript` and is run by
  `mise run script-smoke-level-path-qt6`. It validates `Level.path` setter
  parity for both `FilePath` objects and strings by assigning a saved raster
  level path and reloading that level through the Qt 6 facade.
- A sixteenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/level_transformers.toonzscript` and is run
  by `mise run script-smoke-level-transformers-qt6`. It validates level-wide
  non-rendering transformer parity for `OutlineVectorizer.vectorize(Level)`,
  `CenterlineVectorizer.vectorize(Level)`, and color-mapped
  `Rasterizer.rasterize(Level)`.
- A seventeenth Qt 6 script fixture exists at
  `toonz/sources/tests/scriptengine/file_path_edges.toonzscript` and is run by
  `mise run script-smoke-filepath-edges-qt6`. It validates FilePath edge-case
  parity for relative concatenation, absolute-path concat rejection,
  non-directory `files()` errors, and directory listing through the Qt 6
  facade.
- The script smoke can run in bounded mode or natural-exit mode. The basic,
  FilePath, FilePath edges, Scene, Scene cells, Scene load-level, Level,
  Level I/O, Image, ImageBuilder, ToonzRasterConverter, OutlineVectorizer,
  CenterlineVectorizer, Rasterizer, Wrapper id, Level path, and level
  transformer fixtures pass in both modes for the current Qt 6 app bundle.
  The script entry path now
  intentionally exits before the normal macOS
  `QApplication`/plugin/main-window startup and uses `stuff/cache` instead of
  the platform cache root so sandboxed CLI smokes do not write into a user
  Library cache.
- After every macOS Qt 6 relink, `mise run package-macos-qt6` is still required
  before GUI smokes. The current package pass is very noisy and slow because
  `macdeployqt` rewrites a large transitive Qt/OpenCV/FFmpeg dependency set
  before re-signing the bundle.
- This is not product-ready Qt 6 support. Drawing workflows, viewer redraw,
  high-DPI behavior, rendering, non-macOS packaging, scripting object binding
  parity, and hardware camera/audio smoke remain open.

## Next Implementation Slice

Do not redo the first runway slice unless the current branch has been discarded.
The next slice should make the Qt 6 app useful enough to run and diagnose:

1. Continue packaged Qt 6 interactive GUI smoke beyond the now-green scripted
   startup/create/open path into raster/vector drawing, xsheet scrubbing, viewer
   redraw, high-DPI behavior, and OpenGL fallback warning triage.
2. Keep the app-side GUI smoke status hook narrow and test-only. System Events
   remains useful as a fallback/manual diagnostic, but do not make Qt 6
   create/open validation depend on macOS accessibility window discovery unless
   that path becomes repeatable again.
3. Extend the script fixture set beyond the current narrow `FilePath`,
   FilePath-edge, `Scene`, `Level`, `Level.path`, level-wide transformer,
   `Image`, `ImageBuilder`, ToonzRasterConverter,
   OutlineVectorizer/CenterlineVectorizer/Rasterizer, and Wrapper id
   compatibility slices into the next `QJSEngine` object-binding
   group, likely scene save/reopen coverage or a non-rendering subset of
   another script binding, rather than attempting a full script API rewrite.
   Full headless scene save/reopen coverage remains open because an initial
   `Scene.save()` fixture reached offscreen scene-icon behavior and timed out
   in the current headless smoke environment.
4. Run focused audio playback/recording and camera enumeration smoke tests on
   real hardware.
5. Keep the basic, FilePath, FilePath-edge, Scene, Scene cells,
   Scene load-level, Level, Level I/O, Level path, level transformer, Image,
   ImageBuilder,
   ToonzRasterConverter, OutlineVectorizer, CenterlineVectorizer, Rasterizer,
   and Wrapper id script fixtures green in both bounded and natural-exit smoke
   modes while adding the next object-binding fixture.
6. Keep broad viewer/rendering work deferred until the Metal checkpoint.

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
5. Finish audio recording and playback validation on real devices.
6. Finish stop-motion camera preview and still capture validation.
7. Continue narrow compile-frontier work in non-rendering targets, preserving
   both Qt 5 and Qt 6 validation.
8. Rebase onto the Metal checkpoint before doing broad viewer/rendering Qt 6
   work.
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
mise run gui-smoke-qt6
mise run gui-smoke-scene-create-qt6
OPENTOONZ_GUI_SMOKE_FILE=<path-to-scene.tnz> bash scripts/qt6/run-gui-smoke.sh
mise run script-smoke-qt6
mise run script-smoke-filepath-qt6
mise run script-smoke-filepath-edges-qt6
mise run script-smoke-scene-qt6
mise run script-smoke-scene-cells-qt6
mise run script-smoke-scene-loadlevel-qt6
mise run script-smoke-level-qt6
mise run script-smoke-level-io-qt6
mise run script-smoke-level-path-qt6
mise run script-smoke-image-qt6
mise run script-smoke-image-builder-qt6
mise run script-smoke-toonz-raster-converter-qt6
mise run script-smoke-outline-vectorizer-qt6
mise run script-smoke-centerline-vectorizer-qt6
mise run script-smoke-rasterizer-qt6
mise run script-smoke-level-transformers-qt6
mise run script-smoke-wrapper-id-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-filepath-edges-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-cells-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-scene-loadlevel-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-io-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-path-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-image-builder-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-toonz-raster-converter-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-outline-vectorizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-centerline-vectorizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-rasterizer-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-level-transformers-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-wrapper-id-qt6
```

Manual smoke validation is required before calling the Qt 6 port product-ready:

- launch OpenToonz
- open an existing scene
- draw raster and vector strokes
- scrub xsheet/timeline
- validate viewer redraw and high-DPI behavior
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
- whether any touched files overlap with the Metal migration boundary

Do not claim Qt 6 support until the Qt 6 lane configures, compiles, packages,
launches, and passes the relevant GUI/hardware smoke checks.
