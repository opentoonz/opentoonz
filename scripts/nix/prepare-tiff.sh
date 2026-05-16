#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
tiff_dir="$repo_root/thirdparty/tiff-4.0.3"
tiff_lib="$tiff_dir/libtiff/.libs/libtiff.a"

if [ "$(uname -s)" = "Darwin" ]; then
  export CC="/usr/bin/cc"
  export CXX="/usr/bin/c++"
  unset NIX_CFLAGS_COMPILE NIX_LDFLAGS NIX_LDFLAGS_BEFORE
  unset NIX_ENFORCE_PURITY
  unset NIX_CC NIX_BINTOOLS
  unset NIX_CC_WRAPPER_TARGET_HOST_arm64_apple_darwin
  unset NIX_CC_WRAPPER_TARGET_HOST_aarch64_apple_darwin
  unset NIX_CC_WRAPPER_TARGET_HOST_x86_64_apple_darwin
  unset NIX_BINTOOLS_WRAPPER_TARGET_HOST_arm64_apple_darwin
  unset NIX_BINTOOLS_WRAPPER_TARGET_HOST_aarch64_apple_darwin
  unset NIX_BINTOOLS_WRAPPER_TARGET_HOST_x86_64_apple_darwin
  unset LIBRARY_PATH CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH
  if [ -z "${OPENTOONZ_MACOS_SDKROOT:-}" ]; then
    for sdk in \
      /Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk \
      /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk \
      /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.sdk \
      /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.4.sdk; do
      if [ -d "$sdk/System/Library/Frameworks/AGL.framework" ]; then
        export OPENTOONZ_MACOS_SDKROOT="$sdk"
        break
      fi
    done
  fi
  if [ -n "${OPENTOONZ_MACOS_SDKROOT:-}" ]; then
    export SDKROOT="$OPENTOONZ_MACOS_SDKROOT"
  fi
fi

jobs="${NIX_BUILD_CORES:-}"
if [ -z "$jobs" ]; then
  if command -v sysctl >/dev/null 2>&1; then
    jobs="$(sysctl -n hw.ncpu)"
  elif command -v nproc >/dev/null 2>&1; then
    jobs="$(nproc)"
  else
    jobs=4
  fi
fi

if [ "${OPENTOONZ_REBUILD_TIFF:-0}" != "1" ] && [ -f "$tiff_lib" ]; then
  echo "Using existing vendored libtiff: $tiff_lib"
  exit 0
fi

if [ ! -x "$tiff_dir/configure" ]; then
  echo "Missing executable configure script at $tiff_dir/configure" >&2
  exit 1
fi

(
  cd "$tiff_dir"
  CFLAGS="${CFLAGS:-} -fPIC" CXXFLAGS="${CXXFLAGS:-} -fPIC" ./configure --disable-jbig --disable-lzma
  make -j"$jobs"
)
