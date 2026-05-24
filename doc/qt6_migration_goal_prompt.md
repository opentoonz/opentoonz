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

As of May 24, 2026, the initial Qt 6 runway is already implemented in this
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
- A bounded startup smoke of the packaged Qt 6 app now gets past the prior
  duplicate-Qt/platform-plugin abort and loads the Qt Multimedia FFmpeg backend.
  OpenGL fallback messages and crash-handler noise after harness termination
  remain to be triaged during interactive GUI smoke.
- A first Qt 6 script smoke fixture exists at
  `toonz/sources/tests/scriptengine/basic.toonzscript` and is run by
  `mise run script-smoke-qt6`. It validates the `QJSEngine` bootstrap for
  `print`, `warning`, and `run` through the packaged Qt 6 app.
- The script smoke can run in bounded mode or natural-exit mode. Natural
  command-line `.toonzscript` process exit now passes with
  `OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6` after the
  Qt 6 `QJSEngine` wrapper was marked as C++-owned.
- This is not product-ready Qt 6 support. Interactive runtime launch,
  non-macOS packaging, scripting object binding parity, hardware camera/audio
  smoke, and rendering validation remain open.

## Next Implementation Slice

Do not redo the first runway slice unless the current branch has been discarded.
The next slice should make the Qt 6 app useful enough to run and diagnose:

1. Run an interactive GUI smoke of the packaged Qt 6 macOS app and capture the
   first real workflow blockers.
2. Verify viewer redraw, high-DPI behavior, OpenGL fallback warnings, and the
   crash-handler `atos` noise observed after bounded smoke termination.
3. Extend the script fixture set into the first `QJSEngine` object-binding
   compatibility slice, starting with narrow files/paths behavior rather than a
   full script API rewrite.
4. Run focused audio playback/recording and camera enumeration smoke tests on
   real hardware.
5. Add a second script fixture that exercises one real OpenToonz object-binding
   group, then keep both bounded and natural-exit smoke modes green.
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
4. Add a text-codec adapter for `QTextCodec` call sites and reduce direct
   `Core5Compat` usage.
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
mise run script-smoke-qt6
OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT=1 mise run script-smoke-qt6
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
