#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
app_path="${OPENTOONZ_APP:-$repo_root/toonz/build/nix-qt6-relwithdebinfo/toonz/OpenToonz.app}"
script_path="${OPENTOONZ_SCRIPT_FIXTURE:-$repo_root/toonz/sources/tests/scriptengine/basic.toonzscript}"
smoke_root="${OPENTOONZ_SCRIPT_SMOKE_ROOT:-$repo_root/toonz/build/qt6-script-smoke}"
timeout_seconds="${OPENTOONZ_SCRIPT_SMOKE_TIMEOUT:-45}"
require_exit="${OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT:-0}"
expected_output="${OPENTOONZ_SCRIPT_EXPECTED_OUTPUT:-qt-script-smoke 3 [true,ok]|qt-script-warning}"

case "$script_path" in
  /*) ;;
  *) script_path="$repo_root/$script_path" ;;
esac

executable="$app_path/Contents/MacOS/OpenToonz"
script_name="$(basename "$script_path")"
log_path="$smoke_root/${script_name%.*}-script-smoke.log"

has_expected_output() {
  local expected
  IFS='|' read -ra expected_lines <<<"$expected_output"
  for expected in "${expected_lines[@]}"; do
    if [[ -n "$expected" ]] && ! grep -Fq "$expected" "$log_path"; then
      return 1
    fi
  done
  return 0
}

is_smoke_process_running() {
  jobs -pr | grep -Fxq "$pid"
}

stop_smoke_process() {
  # OpenToonz handles SIGTERM as a crash signal; bounded smokes use SIGKILL so
  # harness cleanup does not generate misleading crash-handler output.
  kill -9 "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}

if [[ ! -x "$executable" ]]; then
  echo "error: OpenToonz executable not found: $executable" >&2
  exit 1
fi

if [[ ! -f "$script_path" ]]; then
  echo "error: script smoke fixture not found: $script_path" >&2
  exit 1
fi

mkdir -p "$smoke_root/home" "$smoke_root/xdg-config" "$smoke_root/xdg-cache"
if [[ ! -d "$smoke_root/stuff" ]]; then
  cp -pR "$repo_root/stuff" "$smoke_root/stuff"
fi

HOME="$smoke_root/home" \
XDG_CONFIG_HOME="$smoke_root/xdg-config" \
XDG_CACHE_HOME="$smoke_root/xdg-cache" \
"$executable" -TOONZROOT "$smoke_root/stuff" "$script_path" \
  >"$log_path" 2>&1 &
pid=$!

elapsed=0
expected_seen=0
while is_smoke_process_running; do
  if has_expected_output; then
    expected_seen=1
    if [[ "$require_exit" != "1" ]]; then
      stop_smoke_process
      echo "Script smoke passed: $script_path"
      exit 0
    fi
  fi

  if ((elapsed >= timeout_seconds)); then
    stop_smoke_process
    if ((expected_seen)); then
      echo "error: script smoke saw expected output but the process did not exit after ${timeout_seconds}s" >&2
    else
      echo "error: script smoke timed out after ${timeout_seconds}s" >&2
    fi
    sed -n '1,160p' "$log_path" >&2
    exit 124
  fi
  sleep 1
  elapsed=$((elapsed + 1))
done

set +e
wait "$pid"
status=$?
set -e

if ((status != 0)); then
  echo "error: script smoke exited with status $status" >&2
  sed -n '1,160p' "$log_path" >&2
  exit "$status"
fi

if ! has_expected_output; then
  echo "error: script smoke did not emit the expected print output" >&2
  sed -n '1,160p' "$log_path" >&2
  exit 1
fi

echo "Script smoke passed: $script_path"
