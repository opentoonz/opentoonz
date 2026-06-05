#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

desktop_widget_hits="$(
  git grep -nE -e 'QDesktopWidget|QApplication::desktop|qApp->desktop|[[:<:]]desktop[[:space:]]*\([[:space:]]*\)' -- \
    'toonz/sources' \
    ':!toonz/sources/translations' || true
)"

if [[ -n "$desktop_widget_hits" ]]; then
  printf '%s\n' "$desktop_widget_hits"
  echo "error: use QScreen/widget screen helpers instead of removed QDesktopWidget/QApplication::desktop APIs" >&2
  exit 1
fi

echo "qt6-desktopwidget-scope-check ok"
