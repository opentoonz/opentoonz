#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

missing_tasks=()
missing_fixtures=()

while IFS= read -r task; do
  [[ -n "$task" ]] || continue
  if ! grep -Fxq "[tasks.${task}]" mise.toml; then
    missing_tasks+=("$task")
  fi
done < <(
  awk '
    /^[[:space:]]*tasks=\([[:space:]]*$/ { in_tasks = 1; next }
    in_tasks && /^[[:space:]]*\)/ { in_tasks = 0; next }
    in_tasks {
      line = $0
      sub(/#.*/, "", line)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
      if (line != "")
        print line
    }
  ' scripts/qt6/run-all-script-smokes.sh
)

while IFS= read -r fixture; do
  [[ -n "$fixture" ]] || continue
  if [[ ! -f "$fixture" ]]; then
    missing_fixtures+=("$fixture")
  fi
done < <(
  rg -o "OPENTOONZ_SCRIPT_(LIBRARY_)?FIXTURE=[^[:space:]']+" mise.toml |
    sed -E 's/^.*OPENTOONZ_SCRIPT_(LIBRARY_)?FIXTURE=//' |
    sort -u
)

if ((${#missing_tasks[@]} > 0)); then
  printf 'missing script-smoke tasks in mise.toml:\n' >&2
  printf '  %s\n' "${missing_tasks[@]}" >&2
  exit 1
fi

if ((${#missing_fixtures[@]} > 0)); then
  printf 'missing script-smoke fixtures:\n' >&2
  printf '  %s\n' "${missing_fixtures[@]}" >&2
  exit 1
fi

echo "qt6-script-smoke-registry-check ok"
