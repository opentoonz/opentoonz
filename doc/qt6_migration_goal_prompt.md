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

## First Implementation Slice

Implement a safe Qt 6 runway, not the whole port.

Required deliverables:

1. Add a CMake cache option such as `OPENTOONZ_QT_MAJOR`, defaulting to `5`.
2. Add central Qt helper variables or functions so subdirectories do not hard
   code `qt5_wrap_cpp`, `qt5_add_resources`, or `qt5_create_translation`
   directly.
3. Add a Qt 6 Nix dependency set next to the existing Qt 5 dependency set.
4. Add Qt 6 CMake presets:
   - `nix-qt6-relwithdebinfo`
   - `nix-qt6-debug`
5. Add mise tasks:
   - `doctor-qt6`
   - `configure-qt6`
   - `build-qt6`
6. Convert a representative subset of CMake targets to version-aware Qt targets
   while preserving the Qt 5 build.
7. Add a diagnostic deprecated-API lane using
   `QT_DISABLE_DEPRECATED_UP_TO=0x050F00`, but do not enable it for the default
   build.
8. Port one contained removed API family, preferably `QDesktopWidget`, behind a
   shared helper.

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

After the first slice, proceed in this order unless the live compile errors
force a narrower dependency order:

1. Keep Qt 5 as the default lane and rerun a focused Qt 5 build after every
   shared-source fix.
2. Port `QRegExp` and `QRegExpValidator` to `QRegularExpression`.
3. Add a text-codec adapter for `QTextCodec` call sites and reduce direct
   `Core5Compat` usage.
4. Replace direct `QGLWidget::convertToGLFormat` calls with a local helper.
5. Finish the scripting abstraction: the Qt 6 `QJSEngine` shell exists, but
   OpenToonz object bindings remain Qt 5-only until they move behind a
   project-owned facade.
6. Finish audio recording and playback validation on real devices.
7. Port stop-motion camera enumeration, preview, and still capture APIs.
8. Continue narrow compile-frontier work in non-rendering targets, preserving
   both Qt 5 and Qt 6 validation.
9. Rebase onto the Metal checkpoint before doing broad viewer/rendering Qt 6
   work.
10. Add Qt 6 packaging lanes for macOS, Linux, and Windows.

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
cmake --build toonz/build/nix-qt6-relwithdebinfo --target OpenToonz
mise run package-macos-qt6
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
