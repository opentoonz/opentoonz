#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_combobox_activated="$(
  git grep -nE -e 'QComboBox::activated|&QComboBox::activated|connect\([^;]*(Combo|ComboBox|Om|CB|m_currentStageObjectCombo)[^;]*SIGNAL\(activated\(int\)\)' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' || true
)"

if [[ -n "$direct_combobox_activated" ]]; then
  printf '%s\n' "$direct_combobox_activated"
  echo "error: use QtCompat::connectComboBoxActivatedIndex() instead of deprecated QComboBox::activated(int)" >&2
  exit 1
fi

if ! git grep -q 'connectComboBoxActivatedIndex' -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::connectComboBoxActivatedIndex() helper in qtcompat.h" >&2
  exit 1
fi

echo "qt6-combobox-activated-scope-check ok"
