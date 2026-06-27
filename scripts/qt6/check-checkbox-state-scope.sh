#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

qtcompat="toonz/sources/include/toonzqt/qtcompat.h"

if ! rg -q "connectCheckStateChanged" "$qtcompat"; then
  echo "Missing QtCompat::connectCheckStateChanged helper in $qtcompat" >&2
  exit 1
fi

if rg -n "SIGNAL\\(stateChanged\\(int\\)\\)|&QCheckBox::stateChanged" \
  toonz/sources \
  --glob '!**/build/**' \
  --glob "!$qtcompat"; then
  cat >&2 <<'MSG'
Direct QCheckBox::stateChanged connections are not allowed in feature code.
Use QtCompat::connectCheckStateChanged() so Qt 6.7+ can use checkStateChanged()
while the Qt 5 lane keeps stateChanged(int).
MSG
  exit 1
fi

echo "qt6-checkbox-state-scope-check ok"
