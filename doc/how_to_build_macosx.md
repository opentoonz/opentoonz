# Setting Up the Development Environment on macOS

## Necessary Software

- git
- Nix and mise for the preferred Apple Silicon build
- brew for the Homebrew fallback
- Xcode
- cmake (3.10 or later)
- Qt 5.x (5.15 or later)
- boost (1.55.0 or later)

## Building on macOS

### Apple Silicon recommended path

For modern macOS and M-series Macs, use the pinned Nix + mise workflow:

```sh
mise run doctor
mise run configure
mise run build
mise run package-macos
mise run check-macos-arm64
mise run dmg-macos
```

The build output is
`toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app`. The architecture check
verifies the main binary, helper tools, and bundled Mach-O libraries are
`arm64` and not x86_64-only.

The default development version for this lane is `1.7.99`, a numeric
placeholder for a 1.7-series Apple Silicon build. Maintainers can choose the
final release number by passing `-DOPENTOONZ_APP_VERSION=...` during configure.
The About dialog build timestamp is generated at configure time instead of
using compiler `__DATE__`, which avoids reproducible-build epochs such as
January 1, 1980.

Packaged builds include default `stuff` data in
`OpenToonz.app/Contents/Resources/portablestuff`. That bundled copy is a
read-only seed for signing and notarization. At runtime, OpenToonz creates and
uses `~/Library/Application Support/OpenToonz/stuff` for writable profiles,
projects, and sandbox files so the app bundle is not modified after signing.

The default macOS package lane still uses Qt 5.15.18 as the native Apple
Silicon bridge. The active Qt 6 migration is maintained as a separate Nix/mise
lane so the Qt 5 build remains available while the Qt 6 port is validated. The
Qt 5 Nix shell also selects a macOS 15 SDK with `AGL.framework` when available
because Qt 5 still references AGL during configuration.

The Nix setup is described in more detail in
[Building With Nix And Mise](./how_to_build_nix_mise.md). That document also
tracks the current Apple Silicon parity gaps compared with the previous Intel
macOS artifact. In short, the default Qt 5 package is arm64-only, Canon DSLR
support is disabled in public builds, pull request artifacts are ad hoc signed
rather than notarized, and Qt 6 validation uses the dedicated Qt 6 lane until it
is ready to replace the Qt 5 bridge.

### Download and install Xcode from Apple

When downloading Xcode, you should use the appropriate version for your OS version.  You can refer to the Version Comparison Table on https://en.wikipedia.org/wiki/Xcode to find out which version you should use.

Apple Store usually provides for the most recent macOS version.  For older versions, you will need to go to the Apple Developer site.

After installing the application, you will need to start it in order to complete the installation.

### Install Homebrew from https://brew.sh

Homebrew remains a fallback for local development. Prefer `brew --prefix`
instead of hard-coded `/usr/local` or `/opt/homebrew` paths because Intel and
Apple Silicon installations use different prefixes.

Check site for any changes in installation instructions, but they will probably just be this:

1. Open a Terminal window
2. Execute the following statement:
```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

### Install required software using brew

In a Terminal window, execute the following statements:
```sh
brew install glew lz4 libjpeg libpng lzo pkg-config libusb cmake git-lfs libmypaint qt@5 boost jpeg-turbo opencv
git lfs install
```

NOTE: This will install the latest version of QT v5.x which may not be compatible with older OS versions.

Homebrew `qt@5` is deprecated upstream, which is one reason the reproducible
Apple Silicon build uses the pinned Nix dependency set for CI and release
artifacts.

If you cannot use the most recent version, download the online installer from https://www.qt.io/download and install the appropriate `macOS` version (min 5.15).  If installing via this method, be sure to install the `Qt Script (Deprecated)` libraries.

### Remove incompatible symbolic directory
Check to see if this symbolic glew directory exists. If so, remove it:
```sh
ls -l "$(brew --prefix)/lib/cmake/glew"
rm "$(brew --prefix)/lib/cmake/glew"
```

### Set up OpenToonz repository

These steps will put the OpenToonz repository under /Users/yourlogin/Documents.
```sh
cd ~/Documents   #or where you want to store the repository#
git clone https://github.com/opentoonz/opentoonz
cd opentoonz
git lfs pull
cd thirdparty/lzo
cp -r 2.03/include/lzo driver
cd ../tiff-4.0.3
./configure --disable-lzma && make
```

If you downloaded and installed boost from https://boost.org instead of homebrew, move the package under `thirdparty/boost` as follows: 
```sh
cd thirdparty/boost
mv ~/Downloads/boost_1_72_0.tar.bz2 .   #or whatever the boost filename you downloaded is#
tar xvjf boost_1_72_0.tar.bz2
```

### Configure environment and Build OpenToonz with Homebrew

1. Create the build directory with the following:
```sh
cd ~/Documents/opentoonz/toonz
mkdir build
cd build
```
2. Include libjpeg-turbo path to PKG_CONFIG_PATH

```sh
export PKG_CONFIG_PATH="$(brew --prefix jpeg-turbo)/lib/pkgconfig:$PKG_CONFIG_PATH"
```

3. Set up build environment

To build from command line, do the following:
```sh
cmake ../sources -G Ninja \
  -DQT_PATH="$(brew --prefix qt@5)/lib" \
  -DQt5_DIR="$(brew --prefix qt@5)/lib/cmake/Qt5" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)/lib/cmake/Qt5" \
  -DWITH_CANON=OFF \
  -DWITH_TRANSLATION=OFF
ninja
```
- If you downloaded the QT installer and installed to `/Users/yourlogin/Qt` instead of by using homebrew, your lib path may look something like this: `~/Qt/5.12.2/clang_64/lib` or `~/Qt/5.12.2/clang_32/lib`

To build using Xcode, do the following:
```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
cmake -G Xcode ../sources -B. \
  -DQT_PATH="$(brew --prefix qt@5)/lib" \
  -DQt5_DIR="$(brew --prefix qt@5)/lib/cmake/Qt5" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)/lib/cmake/Qt5" \
  -DWITH_CANON=OFF \
  -DWITH_TRANSLATION=OFF
```
- Note that the option `-DWITH_TRANSLATION=OFF` is needed to avoid error when using XCode 12+ which does not allow to add the same source to multiple targets.
- Open Xcode app and open project /Users/yourlogin/Documents/opentoonz/toonz/build/OpenToonz.xcodeproj
- Change `ALL_BUILD` to `OpenToonz`
- Start build with: Product -> Build

    - NOTE about rebuilding in Xcode: The initial build should succeed without any errors.  There after, the build will succeed but the following 3 errors can be ignored:

```
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/install_name_tool: for: /Users/yourlogin/Documents/opentoonz/toonz/build/toonz/Debug/OpenToonz.app/Contents/MacOS/OpenToonz (for architecture x86_64) option "-add_rpath @executable_path/." would duplicate path, file already has LC_RPATH for: @executable_path/.
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/install_name_tool: for: /Users/yourlogin/Documents/opentoonz/toonz/build/toonz/Debug/OpenToonz.app/Contents/MacOS/OpenToonz (for architecture x86_64) option "-add_rpath /usr/local/Cellar/qt/5.12.2/lib/" would duplicate path, file already has LC_RPATH for: /usr/local/Cellar/qt/5.12.2/lib/
Command /bin/sh emitted errors but did not return a nonzero exit code to indicate failure
```

Side note: If you want the option to build by command line and Xcode, create a separate build directory for each.

### Create the stuff Directory

If you have installed OpenToonz on the machine already, you can skip this.  Otherwise, you need to create the stuff folder with the following:
```sh
cd ~/Documents/opentoonz
sudo mkdir /Applications/OpenToonz
sudo cp -r stuff /Applications/OpenToonz/OpenToonz_stuff
sudo chmod -R 777 /Applications/OpenToonz
```

### Running the build

- If built using command line, run the following:
```sh
open ~/Documents/opentoonz/toonz/build/toonz/OpenToonz.app
```

- If built using Xcode, do the following:

    - Open Scheme editor for OpenToonz: Product -> Scheme -> Edit Scheme
    - Uncheck: Run -> Options -> Document Versions
    - Run in Debug mode: Product -> Run

    - To open with command line or from Finder window, the application is found in `/Users/yourlogin/Documents/opentoonz/toonz/build/Debug/OpenToonz.app`

### Canon DSLR camera support

Canon support is intentionally disabled for public macOS arm64 builds with
`WITH_CANON=OFF`. The Canon EDSDK is gated and should not be committed to the
repository or required for CI. Maintainers with access to an arm64-capable EDSDK
can add a separate private build lane later.

### Current Apple Silicon feature gaps

The native arm64 build is meant to cover normal OpenToonz drawing, scene,
xsheet, FX, render, file I/O, and helper-tool workflows. The current known gaps
against the last Intel macOS artifact are:

- the default artifact is arm64-only, not Universal 2
- Canon DSLR capture is disabled in public builds
- release-grade Developer ID signing and notarization require maintainer
  secrets and are not present on pull request artifacts
- Qt 6 validation is in the dedicated Nix/mise Qt 6 lane and is not part of the
  default Qt 5 Apple Silicon package pass
- Metal rendering and full removal of the legacy OpenGL/AGL stack remain
  separate follow-up work after the Qt 6 port reaches parity
- scanner, tablet, webcam, and stop-motion hardware need real-device regression
  testing before declaring full parity
