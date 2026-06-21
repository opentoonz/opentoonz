#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

legacy_appkit_event_hits="$(
  git grep -nE -e '\bNS(Left|Right|Other)Mouse(Down|Up|Dragged)\b' -- \
    'toonz/sources' \
    ':!toonz/sources/translations' || true
)"

if [[ -n "$legacy_appkit_event_hits" ]]; then
  printf '%s\n' "$legacy_appkit_event_hits"
  echo "error: use NSEventType* constants instead of deprecated AppKit NS*Mouse* constants" >&2
  exit 1
fi

echo "qt6-appkit-event-scope-check ok"
