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

if [[ -n "$source_hits" || -n "$state_hits" ]]; then
  [[ -z "$source_hits" ]] || printf '%s\n' "$source_hits"
  [[ -z "$state_hits" ]] || printf '%s\n' "$state_hits"
  echo "error: use QtCompat media-player helpers instead of direct Qt 6 QMediaPlayer source/state APIs" >&2
  exit 1
fi

echo "qt6-mediaplayer-scope-check ok"
