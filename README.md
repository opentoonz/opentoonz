# Flare

A fork of Flare rebranded as Flare — focused on providing an Adobe Animate-like
user experience and improving interoperability with Flash assets (.swf/.fla).

This repository is a fork of Flare and retains the original licensing and
attribution. See the Licensing section below for details.

[日本語](./doc/README_ja.md) [简体中文](./doc/README_chs.md)

## What is Flare?

Flare is a community-driven fork of Flare that ships a revamped UI layout
inspired by Adobe Animate and introduces native import support for SWF files
(when FFmpeg is available) and experimental helpers for FLA projects.

For the original Flare project and its history, see the Flare website:
https://Flare.github.io/e/index.html

## Program Requirements

To enable SWF import features, install FFmpeg and ensure it is available on your PATH.

## Installation

Please see the `doc/` folder for platform-specific build and installation
instructions.

## How to Build Locally

- [Windows](./doc/how_to_build_win.md)
- [macOS](./doc/how_to_build_macosx.md)
- [Linux](./doc/how_to_build_linux.md)
- [BSD](./doc/how_to_build_bsd.md)

## Community & Contribution

This fork aims to stay compatible with Flare where possible while
introducing new features. When contributing, please keep the original project's
licensing and attribution in mind.

## Licensing

Files outside of the `thirdparty` and `stuff/library/mypaint brushes`
directories are based on the Modified BSD License.
- [modified BSD license](./LICENSE.txt).

Third-party components retain their original licenses. See the relevant
documentation in `thirdparty/` and `stuff/library/mypaint brushes/Licenses.txt`.

### Adobe Animate-style Workspace & Theme

Flare includes an "Adobe Animate" workspace and an Adobe-like color theme by
default. To switch to the Adobe Animate workspace, open the Room (Workspace)
menu and choose "Adobe Animate".

### Importing SWF files

Flare uses FFmpeg to import `.swf` files as raster frame sequences when
FFmpeg supports the format on your system. Install FFmpeg and ensure it is
available on your PATH (or configure it via Preferences) to enable SWF import
capabilities.

Flare also provides a Vector Flash import workflow that leverages third-party
Flash decompilers (e.g., JPEXS). Use File → Import → Import Flash (Vector via
External Decompiler)... to run a helper script that exports SVG/image sequences
from Flash files for importing into Flare. See `doc/how_to_import_swf.md` for
details.

