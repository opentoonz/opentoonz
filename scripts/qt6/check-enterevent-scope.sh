#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_enter_event_hits="$(
  git grep -nE -e 'enterEvent[[:space:]]*\([[:space:]]*(QEvent|QEnterEvent)[[:space:]]*\*' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations' || true
)"

if [[ -n "$direct_enter_event_hits" ]]; then
  printf '%s\n' "$direct_enter_event_hits"
  echo "error: use QtCompat::EnterEvent for enterEvent overrides instead of direct QEvent/QEnterEvent signatures" >&2
  exit 1
fi

if ! git grep -q 'using EnterEvent' -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::EnterEvent alias in qtcompat.h" >&2
  exit 1
fi

echo "qt6-enterevent-scope-check ok"
