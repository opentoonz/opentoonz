#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

qt_qsound_boundary='(^|[^[:alnum:]_])QSound([^[:alnum:]_]|$)'
qt_video_surface_boundary='(^|[^[:alnum:]_])(QAbstractVideoSurface|QVideoSurfaceFormat)([^[:alnum:]_]|$)'
legacy_penciltest_condition='if\(\(PLATFORM EQUAL 64\) OR \(OPENTOONZ_QT_MAJOR EQUAL 6\)\)'

unexpected_qsound="$(
  git grep -nE "$qt_qsound_boundary" -- 'toonz/sources' || true
)"
if [[ -n "$unexpected_qsound" ]]; then
  printf '%s\n' "$unexpected_qsound"
  echo "error: use QSoundEffect instead of the removed Qt 5 QSound API" >&2
  exit 1
fi

unexpected_video_surface="$(
  git grep -nE "$qt_video_surface_boundary" -- \
    'toonz/sources' \
    ':!toonz/sources/toonz/penciltestpopup_qt.cpp' \
    ':!toonz/sources/toonz/penciltestpopup_qt.h' || true
)"
if [[ -n "$unexpected_video_surface" ]]; then
  printf '%s\n' "$unexpected_video_surface"
  echo "error: Qt 5 video-surface APIs must stay confined to the legacy 32-bit penciltestpopup_qt fallback" >&2
  exit 1
fi

if ! grep -qE "$legacy_penciltest_condition" toonz/sources/toonz/CMakeLists.txt; then
  echo "error: legacy penciltestpopup_qt sources must be excluded from the Qt 6 lane" >&2
  exit 1
fi

echo "qt6-multimedia-scope-check ok"
