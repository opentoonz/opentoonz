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

require_helper() {
  local helper="$1"

  if ! git grep -q "$helper" -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
    echo "error: missing QtCompat helper: $helper" >&2
    exit 1
  fi
}

require_helper 'graphicsSceneContextMenuEventPositionF'
require_helper 'graphicsSceneContextMenuEventGlobalPosition'

direct_context_menu_positions="$(
  grep_hits git grep -nE \
    -e '(^|[^[:alnum:]_])(cme|gce|graphicsSceneContextMenuEvent|sceneContextMenuEvent)->[[:space:]]*(pos|screenPos)[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$direct_context_menu_positions" ]]; then
  printf '%s\n' "$direct_context_menu_positions"
  echo "error: use QtCompat graphics-scene context-menu position helpers instead of direct pos()/screenPos()" >&2
  exit 1
fi

echo "qt6-graphicsscenecontextmenu-scope-check ok"
