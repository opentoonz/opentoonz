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

tablet_event_position_hits="$(
  grep_hits git grep -nE \
    -e '->[[:space:]]*posF[[:space:]]*\(' \
    -e '->[[:space:]]*globalPos[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

tablet_event_constructor_hits="$(
  grep_hits git grep -nE '(^|[^[:alnum:]_:])QTabletEvent[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$tablet_event_position_hits" ]]; then
  printf '%s\n' "$tablet_event_position_hits"
  echo "error: use QtCompat tablet-event position helpers instead of direct QTabletEvent posF()/globalPos()" >&2
  exit 1
fi

if [[ -n "$tablet_event_constructor_hits" ]]; then
  printf '%s\n' "$tablet_event_constructor_hits"
  echo "error: use QtCompat::makeTabletEvent() instead of direct QTabletEvent construction" >&2
  exit 1
fi

echo "qt6-tabletevent-scope-check ok"
