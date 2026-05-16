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
