#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

desktop_widget_hits="$(
  git grep -nE -e 'QDesktopWidget|QApplication::desktop|qApp->desktop|[[:<:]]desktop[[:space:]]*\([[:space:]]*\)' -- \
    'toonz/sources' \
    ':!toonz/sources/translations' || true
)"

primary_screen_hits="$(
  git grep -nE -e '(QGuiApplication|QApplication)::primaryScreen[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/toonzqt/gutil.cpp' \
    ':!toonz/sources/toonz/main.cpp' \
    ':!toonz/sources/translations' || true
)"

dialog_screen_hits="$(
  git grep -nE -e 'QGuiApplication::screenAt|QGuiApplication::screens[[:space:]]*\([[:space:]]*\).*availableGeometry' -- \
    'toonz/sources/toonzqt/dvdialog.cpp' || true
)"

if [[ -n "$desktop_widget_hits" || -n "$primary_screen_hits" ||
      -n "$dialog_screen_hits" ]]; then
  printf '%s\n' "$desktop_widget_hits"
  printf '%s\n' "$primary_screen_hits"
  printf '%s\n' "$dialog_screen_hits"
  echo "error: use QScreen/widget screen helpers instead of removed desktop APIs and direct primary-screen fallbacks" >&2
  exit 1
fi

echo "qt6-desktopwidget-scope-check ok"
