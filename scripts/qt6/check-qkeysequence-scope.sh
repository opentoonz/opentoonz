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

qkeysequence_hits="$(
  grep_hits git grep -nE -e '[.]toCombined[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

indexed_conversion_hits="$(
  grep_hits git grep -nE -e 'return[[:space:]]+(ks|keySequence|sequence)[[:space:]]*\[[[:space:]]*[0-9]+[[:space:]]*\][[:space:]]*;' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$qkeysequence_hits" || -n "$indexed_conversion_hits" ]]; then
  printf '%s\n' "$qkeysequence_hits"
  printf '%s\n' "$indexed_conversion_hits"
  echo "error: use QtCompat::keySequenceEntryToInt() instead of direct QKeySequence entry conversion" >&2
  exit 1
fi

echo "qt6-qkeysequence-scope-check ok"
