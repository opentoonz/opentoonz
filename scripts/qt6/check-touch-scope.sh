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

touch_hits="$(
  grep_hits git grep -nE \
    -e '->touchPoints[[:space:]]*\(' \
    -e '[.]touchPoints[[:space:]]*\(' \
    -e '->[[:space:]]*device[[:space:]]*\([[:space:]]*\)[[:space:]]*->[[:space:]]*type[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$touch_hits" ]]; then
  printf '%s\n' "$touch_hits"
  echo "error: use QtCompat touch helpers instead of direct QTouchEvent touchPoints() or device()->type() access" >&2
  exit 1
fi

echo "qt6-touch-scope-check ok"
