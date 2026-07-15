#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_qaction_parent_widget="$(
  git grep -nE -e '\b(act|action|menuAction|qaction)[[:alnum:]_]*->[[:space:]]*parentWidget[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' || true
)"

if [[ -n "$direct_qaction_parent_widget" ]]; then
  printf '%s\n' "$direct_qaction_parent_widget"
  echo "error: use QAction::parent() with qobject_cast() instead of deprecated QAction::parentWidget()" >&2
  exit 1
fi

echo "qt6-qaction-scope-check ok"
