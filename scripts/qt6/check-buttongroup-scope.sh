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

direct_buttongroup_id_signals="$(
  grep_hits git grep -nE \
    -e 'SIGNAL[[:space:]]*\([[:space:]]*button(Clicked|Pressed|Released|Toggled)[[:space:]]*\([[:space:]]*int[[:space:]]*\)' \
    -e '&QButtonGroup::button(Clicked|Pressed|Released|Toggled)' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/translations'
)"

if [[ -n "$direct_buttongroup_id_signals" ]]; then
  printf '%s\n' "$direct_buttongroup_id_signals"
  echo "error: use QtCompat QButtonGroup id signal helpers instead of direct legacy int button signals" >&2
  exit 1
fi

echo "qt6-buttongroup-scope-check ok"
