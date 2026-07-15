#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_combobox_activated="$(
  git grep -nE -e 'QComboBox::activated|&QComboBox::activated|connect\([^;]*(Combo|ComboBox|Om|CB|m_currentStageObjectCombo)[^;]*SIGNAL\(activated\(int\)\)' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' || true
)"

direct_combobox_text_activated="$(
  git grep -nE -e 'connect\([^;]*(Combo|ComboBox|Om|CB|m_currentStageObjectCombo)[^;]*SIGNAL\(activated\((const[[:space:]]+)?QString[[:space:]]*&?\)\)' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' || true
)"

if [[ -n "$direct_combobox_activated" || -n "$direct_combobox_text_activated" ]]; then
  printf '%s\n' "$direct_combobox_activated"
  printf '%s\n' "$direct_combobox_text_activated"
  echo "error: use QtCompat combo-box activation helpers instead of deprecated QComboBox::activated(...)" >&2
  exit 1
fi

if ! git grep -q 'connectComboBoxActivatedIndex' -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::connectComboBoxActivatedIndex() helper in qtcompat.h" >&2
  exit 1
fi

if ! git grep -q 'connectComboBoxTextActivated' -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::connectComboBoxTextActivated() helper in qtcompat.h" >&2
  exit 1
fi

echo "qt6-combobox-activated-scope-check ok"
