#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

if git grep -nE '\bQRegExp(Validator)?\b' -- 'toonz/sources'; then
  echo "error: Qt 6 migration forbids QRegExp/QRegExpValidator in toonz/sources" >&2
  exit 1
fi

echo "qregexp-check ok"
