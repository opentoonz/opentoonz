#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

qt_qsound_boundary='(^|[^[:alnum:]_])QSound([^[:alnum:]_]|$)'
qt_video_surface_boundary='(^|[^[:alnum:]_])(QAbstractVideoSurface|QVideoSurfaceFormat)([^[:alnum:]_]|$)'
legacy_penciltest_condition='if\(\(PLATFORM EQUAL 64\) OR \(OPENTOONZ_QT_MAJOR EQUAL 6\)\)'
qt_audio_format_codec='(^|[^[:alnum:]_])setCodec[[:space:]]*\([[:space:]]*"audio/pcm"[[:space:]]*\)'

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

unexpected_audio_format_codec="$(
  git grep -nE "$qt_audio_format_codec" -- \
    'toonz/sources' \
    ':!toonz/sources/common/tsound/tsound_qt.cpp' \
    ':!toonz/sources/toonz/audiorecordingpopup.cpp' \
    ':!toonz/sources/toonzlib/txshsoundcolumn.cpp' || true
)"
audio_format_codec_hits="$(
  git grep -nE "$qt_audio_format_codec" -- 'toonz/sources' || true
)"
if [[ -n "$unexpected_audio_format_codec" ]]; then
  printf '%s\n' "$unexpected_audio_format_codec"
  echo "error: QAudioFormat::setCodec() was removed in Qt 6; keep PCM codec setup confined to audited Qt 5-only audio branches" >&2
  exit 1
fi

for expected_audio_format_codec in \
  'toonz/sources/common/tsound/tsound_qt.cpp:[0-9]+:.*format\.setCodec\("audio/pcm"\)' \
  'toonz/sources/toonz/audiorecordingpopup.cpp:[0-9]+:.*format\.setCodec\("audio/pcm"\)' \
  'toonz/sources/toonzlib/txshsoundcolumn.cpp:[0-9]+:.*qFormat\.setCodec\("audio/pcm"\)'; do
  if ! grep -qE "$expected_audio_format_codec" <<<"$audio_format_codec_hits"; then
    echo "error: expected audited Qt 5-only QAudioFormat::setCodec() site missing: $expected_audio_format_codec" >&2
    exit 1
  fi
done

echo "qt6-multimedia-scope-check ok"
