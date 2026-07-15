#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

qt_identifier_boundary='(^|[^[:alnum:]_])QRegExp(Validator)?([^[:alnum:]_]|$)'

if git grep -nE "$qt_identifier_boundary" -- 'toonz/sources'; then
  echo "error: Qt 6 migration forbids QRegExp/QRegExpValidator in toonz/sources" >&2
  exit 1
fi

echo "qregexp-check ok"
