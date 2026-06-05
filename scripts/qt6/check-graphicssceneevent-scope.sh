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

direct_graphics_scene_event_positions="$(
  grep_hits git grep -nE \
    -e '->[[:space:]]*screenPos[[:space:]]*\(' \
    -e '->[[:space:]]*lastScreenPos[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$direct_graphics_scene_event_positions" ]]; then
  printf '%s\n' "$direct_graphics_scene_event_positions"
  echo "error: use QtCompat graphics-scene event position helpers instead of direct screenPos()/lastScreenPos()" >&2
  exit 1
fi

echo "qt6-graphicssceneevent-scope-check ok"
