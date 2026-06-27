#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

legacy_macro_hits="$(
  git grep -nE -e '\b(Q_DECL_OVERRIDE|Q_DECL_FINAL|Q_NULLPTR)\b' -- \
    'toonz/sources' \
    ':!toonz/sources/translations' || true
)"

if [[ -n "$legacy_macro_hits" ]]; then
  printf '%s\n' "$legacy_macro_hits"
  echo "error: use standard C++17 override/final/nullptr instead of legacy Qt compatibility macros" >&2
  exit 1
fi

echo "qt6-modern-qt-macros-check ok"
