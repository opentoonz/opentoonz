# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

OpenToonz is a large C++17 / Qt 5 desktop application for 2D animation. See `AGENTS.md` for additional agent guidance — its instructions apply here too.

## Build Commands

Preferred reproducible workflow on this machine is Nix + mise (Nix owns the C++ dependencies; mise is just a task runner):

```sh
mise run doctor      # verify toolchain/dependency versions in the Nix shell
mise run configure   # prepare vendored libtiff + cmake --preset nix-relwithdebinfo
mise run build       # configure + ninja build into toonz/build/nix-relwithdebinfo
mise run run         # launch the built app/bundle
mise run format      # clang-format files changed from master (beautification.sh)
mise run check       # nix flake check + git diff --check
```

CMake presets live in `toonz/sources/CMakePresets.json` (`nix-relwithdebinfo`, `nix-debug`); the default build dir is `toonz/build/nix-relwithdebinfo` and `compile_commands.json` is exported there.

Manual CMake shape (platform docs in `doc/how_to_build_*.md` cover setup details):

```sh
cd toonz/build
cmake ../sources -G Ninja -DWITH_TRANSLATION=OFF
ninja                       # or: ninja <target> for a single library/app target
```

Build notes:
- Requires Qt 5.15+ (Qt 5 series) and C++17. `WITH_TRANSLATION=OFF` is standard in CI to avoid translation project generation issues.
- The vendored `thirdparty/tiff-4.0.3` must be built before configuring (`scripts/nix/prepare-tiff.sh` handles this in the Nix flow).
- macOS Homebrew builds pass `QT_PATH`/`Qt5_DIR`/`CMAKE_PREFIX_PATH` pointing at `$(brew --prefix qt@5)`.

## Testing / Validation

There is no repo-wide unit test suite. Validate by building the affected target and launching OpenToonz to manually exercise the touched UI/scene/FX/tool behavior. On Linux, runtime testing expects `stuff` copied to `~/.config/OpenToonz/stuff`. If a full build isn't feasible, say so explicitly and include the configure/build command that should be run.

## Formatting

- C++ is formatted with `toonz/sources/.clang-format` (Google-based, C++17, spaces, left pointer alignment, includes NOT sorted).
- From `toonz/sources`, run `./beautification.sh` to format only files changed from `master`. Avoid broad formatting sweeps.

## Architecture

All C++ lives under `toonz/sources/` (the CMake entry point). The library dependency chain, lowest to highest:

- `tnzcore` — base engine: raster/vector primitives, geometry, streams, common utilities.
- `tnzbase` — FX/rendering framework, parameters, scene-level abstractions on top of tnzcore.
- `tnzext` — extensions (e.g. vector deformation/manipulation).
- `toonzlib` — core application logic: scene model, xsheet, levels, palettes, project management.
- `toonzqt` — shared Qt widgets and UI infrastructure used across executables.
- `tnztools` — interactive drawing/editing tools (brush, fill, selection, etc.).
- `image` / `sound` — image and audio format I/O.
- `stdfx` and `plugins/` — built-in effects and the plugin SDK.
- `toonz` — the main GUI application that ties everything together.

Other executables built from the same tree: `tcleanupper`, `tcomposer`, `tconverter` (batch CLI tools) and `toonzfarm` (render farm).

Outside the source tree:
- `stuff/` — runtime application data (default rooms, stylesheets in `stuff/config/qss`, FX layouts, shaders, brushes) copied into installs. Changes here are product behavior changes, not incidental data edits.
- `thirdparty/` — vendored dependencies and prebuilt platform binaries. Do not refactor or reformat unless the task is specifically about that dependency. `.lib`/`.dll` files are Git LFS-tracked; run `git lfs pull` when a platform build needs them, and avoid touching LFS binaries during source-only changes.
- `toonz/sources/translations/` — Qt Linguist `.ts` sources; user-visible string changes may need updates here. Generated `.qm` files belong under `stuff/config/loc` only when intentionally updating packaged translations.
- `.github/workflows/` — CI recipes for Linux/macOS/Windows; compare against these for packaging or dependency changes.

## Active Work: Qt 6 Port

A Qt 5 → Qt 6 port is in progress on the separate `codex/qt-6-port` branch (master is still Qt 5). Progress reports and manual verification checklists live in `doc/qt6_port_progress/`, and Qt 6 guard scripts live in `scripts/qt6/`. Don't assume Qt 6 APIs are available on `master`.

## Conventions

- This is an old, cross-platform codebase with many local conventions — match nearby code before introducing new abstractions, and keep changes scoped.
- Preserve CRLF line endings for `.bat` files (`.gitattributes` enforces this).
- Branch names follow `fix/...` / `feature/...`; PRs target `master` upstream.
- If `git status` trips over LFS filters, use a read-only status command that disables the LFS clean/smudge filters for inspection only.
