#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

unexpected_setcodec="$(
  git grep -nE '(^|[^[:alnum:]_])setCodec[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/common/tsound/tsound_qt.cpp' \
    ':!toonz/sources/toonz/audiorecordingpopup.cpp' \
    ':!toonz/sources/toonzlib/txshsoundcolumn.cpp' || true
)"
if [[ -n "$unexpected_setcodec" ]]; then
  printf '%s\n' "$unexpected_setcodec"
  echo "error: use QtCompat::setTextStreamUtf8() instead of direct QTextStream::setCodec()" >&2
  exit 1
fi

echo "qt6-qtextstream-scope-check ok"
