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

mouse_event_position_hits="$(
  grep_hits git grep -nE \
    -e '(^|[^[:alnum:]_])(event|e|me|mouseEvent)->[[:space:]]*(x|y|pos|globalPos|globalX|globalY|windowPos|scenePosition)[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$mouse_event_position_hits" ]]; then
  printf '%s\n' "$mouse_event_position_hits"
  echo "error: use QtCompat mouse/context/drop-event position helpers instead of direct event coordinate accessors" >&2
  exit 1
fi

drop_event_modifier_hits="$(
  grep_hits git grep -nE \
    -e '(^|[^[:alnum:]_])(dropEvent|drop|de)->[[:space:]]*(keyboardModifiers|modifiers)[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$drop_event_modifier_hits" ]]; then
  printf '%s\n' "$drop_event_modifier_hits"
  echo "error: use QtCompat::dropEventModifiers() instead of direct QDropEvent modifier access" >&2
  exit 1
fi

for helper in mouseEventPosition mouseEventPositionF mouseEventWindowPositionF \
  mouseEventGlobalPosition contextMenuEventPosition \
  contextMenuEventGlobalPosition dropEventPosition dropEventModifiers; do
  if ! git grep -q "$helper" -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
    echo "error: expected QtCompat::$helper() helper in qtcompat.h" >&2
    exit 1
  fi
done

echo "qt6-mouseevent-scope-check ok"
