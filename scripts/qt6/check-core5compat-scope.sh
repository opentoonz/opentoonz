#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

unexpected_qtextcodec="$(
  git grep -nE '\bQTextCodec\b' -- 'toonz/sources' \
    ':!toonz/sources/include/ttextcodec.h' || true
)"
if [[ -n "$unexpected_qtextcodec" ]]; then
  printf '%s\n' "$unexpected_qtextcodec"
  echo "error: direct QTextCodec use must stay behind toonz/sources/include/ttextcodec.h" >&2
  exit 1
fi

unexpected_ttextcodec_include="$(
  git grep -nE '#[[:space:]]*include[[:space:]]+"ttextcodec\.h"' -- \
    'toonz/sources' \
    ':!toonz/sources/image/psd/tiio_psd.cpp' \
    ':!toonz/sources/toonz/sxfio.cpp' \
    ':!toonz/sources/toonzlib/txshsimplelevel.cpp' || true
)"
if [[ -n "$unexpected_ttextcodec_include" ]]; then
  printf '%s\n' "$unexpected_ttextcodec_include"
  echo "error: update the Core5Compat scope guard before adding a new ttextcodec.h consumer" >&2
  exit 1
fi

unexpected_core5compat_link="$(
  git grep -nE 'Core5Compat|QT_CORE5COMPAT(_LINK_TARGETS|_TARGET)?' -- \
    'toonz/sources' \
    ':!toonz/sources/CMakeLists.txt' \
    ':!toonz/sources/image/CMakeLists.txt' \
    ':!toonz/sources/toonz/CMakeLists.txt' \
    ':!toonz/sources/toonzlib/CMakeLists.txt' || true
)"
if [[ -n "$unexpected_core5compat_link" ]]; then
  printf '%s\n' "$unexpected_core5compat_link"
  echo "error: Core5Compat must stay scoped to the documented adapter-owning targets" >&2
  exit 1
fi

echo "core5compat-scope-check ok"
