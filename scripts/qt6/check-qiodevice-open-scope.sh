#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

ignored_open_results="$(
  git grep -nE '\.open[[:space:]]*\([[:space:]]*(QIODevice|QFile)::' -- \
    'toonz/sources' \
    ':!toonz/sources/common/tcontenthistory.cpp' \
    ':!toonz/sources/translations' |
    awk '
      /^[^:]+:[0-9]+:[[:space:]]*(if|while|for)[[:space:]]*\(/ { next }
      /^[^:]+:[0-9]+:.*(&&|\|\|).*\.open[[:space:]]*\(/ { next }
      /^[^:]+:[0-9]+:[[:space:]]+.*\.open[[:space:]]*\(.*\)[[:space:]]*\)[[:space:]]*\{/ { next }
      /^[^:]+:[0-9]+:.*=[^=].*\.open[[:space:]]*\(/ { next }
      /^[^:]+:[0-9]+:[[:space:]]*return[[:space:]]+.*\.open[[:space:]]*\(/ { next }
      { print }
    ' || true
)"

if [[ -n "$ignored_open_results" ]]; then
  printf '%s\n' "$ignored_open_results"
  echo "error: check QIODevice/QFile open() return values instead of ignoring them" >&2
  exit 1
fi

echo "qt6-qiodevice-open-scope-check ok"
