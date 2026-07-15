#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

missing_tasks=()
missing_actions=()
missing_handlers=()
unregistered_tasks=()
registered_tasks=()

task_block() {
  local task="$1"
  awk -v task="$task" '
    $0 == "[tasks." task "]" { in_task = 1; print; next }
    in_task && /^\[tasks\./ { exit }
    in_task { print }
  ' mise.toml
}

while IFS= read -r task; do
  [[ -n "$task" ]] || continue
  registered_tasks+=("$task")
  if ! grep -Fxq "[tasks.${task}]" mise.toml; then
    missing_tasks+=("$task")
    continue
  fi

  block="$(task_block "$task")"
  action="$(printf '%s\n' "$block" |
    sed -n 's/.*OPENTOONZ_GUI_SMOKE_ACTION=\([^[:space:]\"]*\).*/\1/p' |
    head -n 1)"

  if [[ "$task" == "gui-smoke-qt6" ]]; then
    if [[ -n "$action" ]]; then
      missing_actions+=("$task should remain the default startup smoke without OPENTOONZ_GUI_SMOKE_ACTION")
    fi
    continue
  fi

  if [[ -z "$action" ]]; then
    missing_actions+=("$task")
    continue
  fi

  if ! grep -Fq "action == \"$action\"" toonz/sources/toonz/main.cpp; then
    missing_handlers+=("$task -> $action")
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
  ' scripts/qt6/run-all-gui-smokes.sh
)

gui_smoke_task_is_registered() {
  local task="$1"
  local registered
  for registered in "${registered_tasks[@]}"; do
    if [[ "$task" == "$registered" ]]; then
      return 0
    fi
  done

  case "$task" in
    gui-smokes-app-qt6 | \
      gui-smoke-viewer-raster-brush-system-events-qt6)
      return 0
      ;;
  esac

  return 1
}

while IFS= read -r task; do
  [[ -n "$task" ]] || continue
  if ! gui_smoke_task_is_registered "$task"; then
    unregistered_tasks+=("$task")
  fi
done < <(
  sed -n 's/^\[tasks\.\(gui-smoke[^]]*\)\]$/\1/p' mise.toml | sort
)

if ((${#missing_tasks[@]} > 0)); then
  printf 'missing GUI smoke tasks in mise.toml:\n' >&2
  printf '  %s\n' "${missing_tasks[@]}" >&2
  exit 1
fi

if ((${#missing_actions[@]} > 0)); then
  printf 'GUI smoke tasks with missing or unexpected actions:\n' >&2
  printf '  %s\n' "${missing_actions[@]}" >&2
  exit 1
fi

if ((${#missing_handlers[@]} > 0)); then
  printf 'GUI smoke task actions without app-side handlers:\n' >&2
  printf '  %s\n' "${missing_handlers[@]}" >&2
  exit 1
fi

if ((${#unregistered_tasks[@]} > 0)); then
  printf 'GUI smoke tasks missing from scripts/qt6/run-all-gui-smokes.sh:\n' >&2
  printf '  %s\n' "${unregistered_tasks[@]}" >&2
  printf 'add the task to the aggregate runner or document it as an exception in check-gui-smoke-registry.sh\n' >&2
  exit 1
fi

echo "qt6-gui-smoke-registry-check ok"
