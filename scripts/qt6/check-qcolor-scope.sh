#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

qcolor_hits="$(
  git grep -nE -e '[.]setNamedColor[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/translations' || true
)"

if [[ -n "$qcolor_hits" ]]; then
  printf '%s\n' "$qcolor_hits"
  echo "error: use QColor(QString) or a local color parsing helper instead of deprecated QColor::setNamedColor()" >&2
  exit 1
fi

echo "qt6-qcolor-scope-check ok"
