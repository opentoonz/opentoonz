# Building With Nix And Mise

This checkout includes a Nix flake for reproducible C++ dependencies and a
`mise.toml` task file for day-to-day commands. Nix owns Qt 5, CMake, Ninja,
pkg-config, native libraries, and the Linux compiler toolchain. Mise is only a
task runner here; it is configured not to resolve or install language runtimes.

On macOS, the Nix shell uses Apple clang from Xcode/Command Line Tools while
keeping the rest of the dependency set in Nix. This avoids Darwin SDK wrapper
issues in older Autoconf probes used by the vendored libtiff tree. The Qt 5
package still references `AGL.framework`, so the shell selects an installed
macOS 15 SDK that contains AGL when one is available.

This is the preferred build path for Apple Silicon. The Homebrew Qt 5 formula
is useful as a local fallback, but the pinned Nix dependency set is the release
and CI baseline for native arm64 builds.

## How The Setup Works

The workflow has three layers:

- `flake.nix` and `nix/opentoonz-env.nix` define the dependency set. This
  includes Qt 5.15.18, CMake, Ninja, pkg-config, OpenCV, MyPaint, LZO, LZ4,
  libusb, FFmpeg dependencies, and the platform-specific compiler/runtime
  environment.
- `toonz/sources/CMakePresets.json` maps the Nix-provided paths into CMake.
  The default preset is `nix-relwithdebinfo`, which writes to
  `toonz/build/nix-relwithdebinfo`, enables system LZO and SuperLU from Nix,
  disables translation target generation for CI parity, and uses the vendored
  OpenToonz libtiff build.
- `mise.toml` is a task runner on top of Nix. It does not install compilers or
  language runtimes. Each task enters `nix develop path:.` and then runs the
  relevant configure, build, package, or validation command.

On macOS, Nix supplies the libraries and tools, but the build still uses Apple
clang and an installed Apple SDK. This keeps the app compatible with the Darwin
system frameworks that Qt and OpenToonz link against. The Nix environment also
exports the Qt plugin directories used by the packaging script, plus both Apple
and GNU libiconv paths so the final bundle can satisfy libraries that expect
different iconv symbol families.

The local and CI flow is intentionally the same:

```sh
mise run doctor
mise run configure
mise run build
mise run package-macos
mise run check-macos-arm64
mise run dmg-macos
```

`mise run package-macos` runs `scripts/macos/package-nix-app.sh`. It copies
`stuff` into `OpenToonz.app/Contents/Resources/portablestuff`, runs
`macdeployqt`, copies missing Nix store dylibs into `Contents/Frameworks`,
rewrites install names to bundle-relative paths, and applies an ad hoc
signature for local validation. Release signing and notarization are separate
and only run when maintainer Apple credentials are available.

## One-Time Setup

Install Nix with flakes enabled and install mise. If you use direnv, run:

```sh
direnv allow
```

Otherwise enter the development shell explicitly:

```sh
nix develop path:.
```

If mise asks to trust the project config, run:

```sh
mise trust ./mise.toml
```

## Common Commands

```sh
mise run doctor
mise run configure
mise run build
mise run package-macos
mise run check-macos-arm64
mise run dmg-macos
mise run run
```

The build directory is `toonz/build/nix-relwithdebinfo`. A debug preset is also
available from inside the Nix shell:

```sh
bash scripts/nix/prepare-tiff.sh
cmake -S toonz/sources --preset nix-debug
cmake --build toonz/build/nix-debug --parallel
```

## Nix Commands

The flake exposes a development shell, a package build, and a configure check:

```sh
nix develop path:.
nix build path:.
nix flake check path:.
```

`nix flake check` intentionally runs a configure-only check instead of a full
OpenToonz build so routine validation stays practical. Use `mise run build` or
`nix build path:.` when you need compile coverage.

## Vendored TIFF

OpenToonz's CMake files intentionally look for the modified libtiff under
`thirdparty/tiff-4.0.3`, so the Nix workflow builds that vendored dependency
before configuring OpenToonz:

```sh
bash scripts/nix/prepare-tiff.sh
```

Set `OPENTOONZ_REBUILD_TIFF=1` to force a rebuild.

## Apple Silicon macOS

The first modern macOS target is an arm64-only Apple Silicon build:

```sh
mise run doctor
mise run configure
mise run build
mise run package-macos
mise run check-macos-arm64
mise run dmg-macos
file toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app/Contents/MacOS/OpenToonz
lipo -archs toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app/Contents/MacOS/OpenToonz
```

`mise run package-macos` copies `stuff` into
`OpenToonz.app/Contents/Resources/portablestuff` and runs `macdeployqt` from
the Nix Qt tools. On Darwin it bundles both the Apple-compatible libiconv and
GNU libiconv from Nix, because some transitive libraries use `_iconv` while
others, such as `libidn2`, require GNU `_libiconv*` symbols.
The bundled `portablestuff` directory is treated as a read-only seed at
runtime. On first launch from a packaged app, OpenToonz creates a writable copy
under `~/Library/Application Support/OpenToonz/stuff` and writes profiles,
projects, and sandbox data there so the signed app bundle remains sealed.
`mise run check-macos-arm64` fails if the main app, helper tools, or bundled
dylibs are missing an `arm64` slice. By default it also rejects universal or
x86_64 slices; set `OPENTOONZ_ALLOW_UNIVERSAL=1` only when intentionally
building a Universal 2 artifact.

The default Apple Silicon development version is `1.7.99`, which represents a
1.7-series modernization build without claiming an official maintainer release
number. Configure with `-DOPENTOONZ_APP_VERSION=...` to set the app-visible
version and macOS bundle version. The displayed build timestamp is generated at
configure time and can be overridden with `-DOPENTOONZ_BUILD_DATE=...` and
`-DOPENTOONZ_BUILD_TIME=...`.

Qt 5.15.18 remains the default Apple Silicon package bridge. Qt 6 work uses the
separate Nix/mise Qt 6 lane so it can be validated without removing the Qt 5
lane prematurely. Replacing the remaining legacy OpenGL/AGL paths is part of
the Qt 6 parity work where it affects the port. Metal rendering remains a
separate follow-up after the Qt 6 port reaches visual and workflow parity.

Canon DSLR support is disabled by default through `WITH_CANON=OFF`. Canon's
newer EDSDK may provide arm64 support, but the SDK is gated and should not be
added to the public dependency set. Maintainers with access to the SDK can
revisit this as a follow-up build lane.

## Apple Silicon Parity And Feature Gaps

The arm64 build is intended to replace the previous Intel-only macOS artifact
for Apple Silicon users, but it is not yet a Universal 2 release. The current
default artifact is arm64-only:

- Apple Silicon Macs run it natively without Rosetta.
- Intel Macs are not supported by the default Nix/CI artifact.
- A Universal 2 package can be added later by building both slices and running
  the arm64 checker with `OPENTOONZ_ALLOW_UNIVERSAL=1`, but that is not the
  first release target.

Known differences from the last Intel macOS package or CI artifact:

- Canon DSLR capture is not included in public arm64 builds. `WITH_CANON`
  remains `OFF` because Canon EDSDK redistribution and arm64 validation require
  maintainer access to Canon's gated SDK.
- Pull request artifacts are ad hoc signed only. They are useful for testing
  after removing quarantine locally, but they are not Gatekeeper-ready release
  artifacts. Developer ID signing, hardened runtime signing, notarization, and
  stapling require maintainer Apple credentials and only run on guarded release
  steps.
- The default arm64 package remains Qt 5 and OpenGL-based. The arm64 port
  removes the Intel/Rosetta requirement, while Qt 6 validation continues in the
  dedicated Qt 6 lane. Metal is deferred until the Qt 6 port reaches visual and
  workflow parity.
- Hardware integrations need focused retesting on Apple Silicon. Webcam and
  non-Canon stop-motion paths build into the arm64 app, but scanner devices,
  tablets, and camera capture should be checked on real hardware before calling
  the arm64 package fully equivalent to the Intel package.
- Localization generation remains disabled in the Nix/CI lane with
  `WITH_TRANSLATION=OFF`, matching the previous macOS CI workflow. If a release
  needs regenerated `.qm` files, add that as a separate release step rather than
  coupling it to the native arm64 bring-up.

No user-facing drawing, scene, level, xsheet, FX, render, farm-helper, or file
format feature is intentionally removed by the arm64 build. Any failure in
those areas should be treated as a porting bug rather than an expected feature
gap.

## macOS Signing And Notarization

Normal CI and pull request builds are not Developer ID signed. The packaging
script applies an ad hoc bundle signature after `macdeployqt` so local artifacts
have a coherent resource seal; release builds can replace it with Developer ID
signing and notarization when maintainer secrets are configured:

```sh
MACOS_DEVELOPER_ID_APPLICATION="Developer ID Application: Example (TEAMID)" \
  scripts/macos/sign-notarize.sh
```

The script signs `OpenToonz.app` with the hardened runtime. When
`OpenToonz.dmg` exists, it signs the DMG and notarizes it using either
`MACOS_NOTARY_KEYCHAIN_PROFILE` or the `MACOS_NOTARY_APPLE_ID`,
`MACOS_NOTARY_TEAM_ID`, and `MACOS_NOTARY_PASSWORD` environment variables.
