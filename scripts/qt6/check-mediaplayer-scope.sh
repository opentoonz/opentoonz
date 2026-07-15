#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

grep_hits() {
  local output
  local status

  set +e
  output="$("$@" 2>&1)"
  status=$?
  set -e

  if [[ $status -gt 1 ]]; then
    printf '%s\n' "$output" >&2
    exit "$status"
  fi

  printf '%s' "$output"
}

if ! git grep -q 'connectMediaPlayerStateChanged' -- 'toonz/sources/include/toonzqt/qtmediacompat.h'; then
  echo "error: expected QtCompat::connectMediaPlayerStateChanged() helper in qtmediacompat.h" >&2
  exit 1
fi

if ! git grep -q 'mediaPlayerHasMedia' -- 'toonz/sources/include/toonzqt/qtmediacompat.h'; then
  echo "error: expected QtCompat::mediaPlayerHasMedia() helper in qtmediacompat.h" >&2
  exit 1
fi

source_hits="$(
  grep_hits git grep -nE -e '(^|[^[:alnum:]_])(m_player|player)[[:space:]]*->[[:space:]]*setSource[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/include/toonzqt/qtmediacompat.h' \
    ':!toonz/sources/translations'
)"

state_hits="$(
  grep_hits git grep -nE -e '(^|[^[:alnum:]_])(m_player|player)[[:space:]]*->[[:space:]]*playbackState[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/include/toonzqt/qtmediacompat.h' \
    ':!toonz/sources/translations'
)"

media_status_hits="$(
  grep_hits git grep -nE -e '(^|[^[:alnum:]_])(m_player|player)[[:space:]]*->[[:space:]]*mediaStatus[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtmediacompat.h' \
    ':!toonz/sources/translations'
)"

signal_hits="$(
  grep_hits git grep -nE \
    -e 'QMediaPlayer::(playbackStateChanged|stateChanged)' \
    -e 'SIGNAL[[:space:]]*\([[:space:]]*stateChanged[[:space:]]*\([[:space:]]*QMediaPlayer::State' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtmediacompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$source_hits" || -n "$state_hits" || -n "$media_status_hits" ||
      -n "$signal_hits" ]]; then
  [[ -z "$source_hits" ]] || printf '%s\n' "$source_hits"
  [[ -z "$state_hits" ]] || printf '%s\n' "$state_hits"
  [[ -z "$media_status_hits" ]] || printf '%s\n' "$media_status_hits"
  [[ -z "$signal_hits" ]] || printf '%s\n' "$signal_hits"
  echo "error: use QtCompat media-player helpers instead of direct QMediaPlayer source/state APIs" >&2
  exit 1
fi

echo "qt6-mediaplayer-scope-check ok"
