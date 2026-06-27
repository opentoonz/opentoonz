#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_wheel_event_delta="$(
  git grep -nE -e '->[[:space:]]*pixelDelta[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' || true
)"

if [[ -n "$direct_wheel_event_delta" ]]; then
  printf '%s\n' "$direct_wheel_event_delta"
  echo "error: use QtCompat::wheelEventPixelDelta() instead of direct QWheelEvent::pixelDelta()" >&2
  exit 1
fi

if ! git grep -q 'wheelEventPixelDelta' -- \
  'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::wheelEventPixelDelta() helper in qtcompat.h" >&2
  exit 1
fi

echo "qt6-wheelevent-scope-check ok"
