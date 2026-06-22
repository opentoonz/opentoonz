#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

preflight_script="scripts/qt6/check-migration-preflight.sh"
docs=(
  doc/qt6_migration_goal_prompt.md
  doc/qt6_remaining_work_and_manual_verification.md
)

map_check_to_task() {
  local check_path="$1"
  local base
  base="$(basename "$check_path" .sh)"

  case "$check_path" in
    scripts/windows/check-msvc-abi.sh)
      printf '%s\n' "check-windows-msvc-abi"
      ;;
    scripts/qt6/check-no-qregexp.sh)
      printf '%s\n' "check-no-qregexp"
      ;;
    scripts/qt6/check-core5compat-scope.sh)
      printf '%s\n' "check-core5compat-scope"
      ;;
    scripts/qt6/check-multimedia-scope.sh)
      printf '%s\n' "check-qt6-multimedia-scope"
      ;;
    scripts/qt6/check-*.sh)
      printf '%s\n' "check-qt6-${base#check-}"
      ;;
    *)
      echo "error: no mise task mapping for preflight check: $check_path" >&2
      return 1
      ;;
  esac
}

readarray -t check_paths < <(
  awk '
    /^[[:space:]]*checks=\(/ { in_checks = 1; next }
    in_checks && /^[[:space:]]*\)/ { in_checks = 0; next }
    in_checks {
      gsub(/^[[:space:]]+|[[:space:]]+$/, "")
      if ($0 != "") print
    }
  ' "$preflight_script"
)

missing=0

for check_path in "${check_paths[@]}"; do
  if [[ ! -f "$check_path" ]]; then
    echo "error: preflight check does not exist: $check_path" >&2
    missing=1
    continue
  fi

  task="$(map_check_to_task "$check_path")"
  command="mise run $task"

  if ! grep -Fq "[tasks.$task]" mise.toml; then
    echo "error: mise.toml is missing task for preflight check: $task" >&2
    missing=1
  fi

  for doc in "${docs[@]}"; do
    if ! grep -Fq "$command" "$doc"; then
      echo "error: $doc is missing documented guardrail command: $command" >&2
      missing=1
    fi
  done
done

if [[ "$missing" -ne 0 ]]; then
  exit 1
fi

echo "qt6-guardrail-docs-check ok"
