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

if ! git grep -q 'hoverEventPosition' -- \
  'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::hoverEventPosition() helper in qtcompat.h" >&2
  exit 1
fi

direct_hover_positions="$(
  grep_hits git grep -nE \
    -e '(^|[^[:alnum:]_])(he|hoverEvent)->[[:space:]]*(pos|oldPos)[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$direct_hover_positions" ]]; then
  printf '%s\n' "$direct_hover_positions"
  echo "error: use QtCompat::hoverEventPosition() instead of direct QHoverEvent position access" >&2
  exit 1
fi

echo "qt6-hoverevent-scope-check ok"
