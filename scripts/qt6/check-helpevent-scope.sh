#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

help_event_position_hits="$(
  git grep -nE -e '(^|[^[:alnum:]_])(event|e|he|helpEvent)->[[:space:]]*(pos|globalPos)[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations' || true
)"

if [[ -n "$help_event_position_hits" ]]; then
  printf '%s\n' "$help_event_position_hits"
  echo "error: use QtCompat::helpEventPosition() / helpEventGlobalPosition() instead of direct QHelpEvent coordinate accessors" >&2
  exit 1
fi

for helper in helpEventPosition helpEventGlobalPosition; do
  if ! git grep -q "$helper" -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
    echo "error: expected QtCompat::$helper() helper in qtcompat.h" >&2
    exit 1
  fi
done

echo "qt6-helpevent-scope-check ok"
