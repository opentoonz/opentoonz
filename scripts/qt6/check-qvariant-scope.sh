#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

grep_hits() {
  local output
  local status

  set +e
  output="$("$@" 2>&1)"
  status=$?
  set -e

  if [[ $status -gt 1 ]]; then
    printf '%s\n' "$output" >&2
    exit "$status"
  fi

  printf '%s' "$output"
}

qvariant_hits="$(
  grep_hits git grep -nE -e '[.]canConvert[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/toonzlib/preferences.cpp' \
    ':!toonz/sources/translations'
)"

if [[ -n "$qvariant_hits" ]]; then
  printf '%s\n' "$qvariant_hits"
  echo "error: use QVariant::canConvert<T>() or a local Qt-version helper instead of non-template canConvert()" >&2
  exit 1
fi

echo "qt6-qvariant-scope-check ok"
