# AGENTS.md

Guidance for AI coding agents working in this OpenToonz checkout.

## Project Overview

OpenToonz is a large C++17 / Qt 5 desktop application for 2D animation. The
build is CMake-based, with the main CMake entry point at `toonz/sources`. The
repository also contains bundled third-party code and binary dependencies, Qt
translation files, application data under `stuff`, and packaging workflows for
Windows, macOS, and Linux.

## Repository Layout

- `toonz/sources/`: main OpenToonz source tree and CMake project.
- `toonz/sources/toonz/`: primary GUI application.
- `toonz/sources/toonzlib/`: core animation, scene, xsheet, and project logic.
- `toonz/sources/toonzqt/`: shared Qt widgets and UI infrastructure.
- `toonz/sources/tnzcore/`, `tnzbase/`, `tnztools/`, `tnzext/`: common engine,
  tool, and extension libraries.
- `toonz/sources/stdfx/` and `plugins/`: effects and plugin code.
- `toonz/sources/image/` and `sound/`: image and audio handling.
- `toonz/sources/translations/`: Qt Linguist `.ts` translation sources.
- `stuff/`: runtime configuration, default rooms, stylesheets, FX layouts,
  shaders, brushes, and other application data copied into installs/packages.
- `thirdparty/`: vendored libraries and prebuilt assets used by platform builds.
- `doc/`: platform build instructions and contributor-facing documentation.
- `.github/workflows/`: CI build recipes for Linux, macOS, and Windows.

## Build Commands

Follow the platform-specific docs in `doc/how_to_build_*.md` when setting up a
new machine. The high-level local build shape is:

```sh
cd toonz
mkdir -p build
cd build
cmake ../sources
cmake --build .
```

Common CI-style variants:

```sh
# Linux
cd toonz/build
cmake ../sources -G Ninja -DWITH_TRANSLATION=OFF
ninja

# macOS with Homebrew Qt 5
cd toonz/build
cmake ../sources -G Ninja \
  -DQT_PATH="$(brew --prefix qt@5)/lib" \
  -DQt5_DIR="$(brew --prefix qt@5)/lib/cmake/Qt5" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)/lib/cmake/Qt5" \
  -DWITH_TRANSLATION=OFF
ninja
```

The preferred reproducible workflow is the Nix + mise setup:

```sh
mise run doctor
mise run configure
mise run build
```

Nix owns the C++ dependency set; mise is only a task runner. The CMake presets
for this path live in `toonz/sources/CMakePresets.json`, and the default build
directory is `toonz/build/nix-relwithdebinfo`.

Important build notes:

- CMake requires C++17 and Qt 5.15 or newer from the Qt 5 series.
- Builds may require `thirdparty/tiff-4.0.3` to be configured and built first;
  see the platform docs.
- Linux runtime testing expects a copied `stuff` directory under
  `~/.config/OpenToonz/stuff` unless using an installed/package layout.
- macOS and Windows packaging copy `stuff` as portable runtime data.
- `WITH_TRANSLATION=OFF` is commonly used in CI and Xcode builds to avoid
  translation project generation issues.

## Validation

There is no small, repo-wide unit test command in this checkout. For code
changes, prefer the smallest practical validation that exercises the changed
area:

- Configure the CMake project for the current platform.
- Build the affected target, or run `ninja` / `cmake --build .` when feasible.
- Launch OpenToonz and manually exercise UI, scene, file, FX, or tool behavior
  touched by the change.
- For packaging or dependency changes, compare with the matching GitHub Actions
  workflow and platform document.

When a full local build is not feasible, state that explicitly in the final
handoff and include the configure/build command that should be run.

## Formatting And Style

- C++ code is formatted with `toonz/sources/.clang-format`.
- From `toonz/sources`, `./beautification.sh` formats C/C++ files changed from
  `master`.
- The clang-format profile is based on Google style, uses C++17, spaces instead
  of tabs, left pointer alignment, and does not sort includes.
- Keep changes scoped. This is an old, cross-platform codebase with many local
  conventions; match nearby code before introducing new abstractions.
- Avoid broad formatting sweeps unless the task is explicitly formatting-only.

## Translation And UI Data

- User-visible strings in Qt code may need updates in
  `toonz/sources/translations`.
- Generated `.qm` files belong under `stuff/config/loc` only when intentionally
  updating packaged translations.
- Stylesheets live under `stuff/config/qss`; follow `doc/how_to_stylesheet.md`
  for stylesheet-specific work.
- Changes under `stuff` are runtime-visible application defaults. Treat them as
  product behavior changes, not incidental data edits.

## Third-Party And Binary Assets

- `thirdparty` contains vendored dependencies and prebuilt platform binaries.
  Do not refactor, reformat, or replace vendored code unless the task is
  specifically about that dependency.
- `.lib` and `.dll` files are tracked through Git LFS. Run `git lfs pull` when a
  platform build needs the real binary contents.
- Avoid touching LFS-tracked binaries during source-only changes. Git status can
  be noisy if the local LFS filter rewrites or cleans binary files.
- Preserve CRLF line endings for `.bat` files; `.gitattributes` enforces this.

## Agent Workflow

1. Read the relevant platform doc, CMake file, and neighboring source before
   editing.
2. Check the worktree before changes. If the normal `git status` path trips over
   LFS filters, use a read-only status command that disables the LFS clean/smudge
   filters for inspection only.
3. Do not revert user changes or normalize unrelated generated, vendored, or
   binary files.
4. Prefer targeted edits and targeted validation over repository-wide churn.
5. In final handoffs, list the files changed and the validation actually run.
