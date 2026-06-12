#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
app_path="${OPENTOONZ_APP:-$repo_root/toonz/build/nix-qt6-relwithdebinfo/toonz/OpenToonz.app}"
script_path="${OPENTOONZ_SCRIPT_FIXTURE:-$repo_root/toonz/sources/tests/scriptengine/basic.toonzscript}"
smoke_root="${OPENTOONZ_SCRIPT_SMOKE_ROOT:-$repo_root/toonz/build/qt6-script-smoke}"
timeout_seconds="${OPENTOONZ_SCRIPT_SMOKE_TIMEOUT:-45}"
runtime_lock_timeout="${OPENTOONZ_SCRIPT_SMOKE_LOCK_TIMEOUT:-$((timeout_seconds * 2))}"
require_exit="${OPENTOONZ_SCRIPT_SMOKE_REQUIRE_EXIT:-0}"
expected_output="${OPENTOONZ_SCRIPT_EXPECTED_OUTPUT:-qt-script-smoke 3 [true,ok]|qt-script-version 7.1|qt-script-warning}"

case "$script_path" in
  /*) ;;
  *) script_path="$repo_root/$script_path" ;;
esac

library_fixture="${OPENTOONZ_SCRIPT_LIBRARY_FIXTURE:-}"
if [[ -z "$library_fixture" && "$(basename "$script_path")" == "basic.toonzscript" ]]; then
  library_fixture="$repo_root/toonz/sources/tests/scriptengine/run_child.toonzscript"
fi
if [[ -n "$library_fixture" ]]; then
  case "$library_fixture" in
    /*) ;;
    *) library_fixture="$repo_root/$library_fixture" ;;
  esac
fi

executable="$app_path/Contents/MacOS/OpenToonz"
script_name="$(basename "$script_path")"
log_path="$smoke_root/${script_name%.*}-script-smoke.log"
qt_plugin_path="${OPENTOONZ_SCRIPT_SMOKE_QT_PLUGIN_PATH:-}"
hidden_qt_conf=""
hidden_qt_frameworks=()
runtime_lock_dir=""
rm -f "$log_path"

restore_hidden_qt_runtime() {
  if [[ -n "$hidden_qt_conf" && -f "$hidden_qt_conf.hidden" ]]; then
    mv "$hidden_qt_conf.hidden" "$hidden_qt_conf"
  fi
  local framework
  for framework in "${hidden_qt_frameworks[@]:-}"; do
    if [[ -d "$framework.hidden" ]]; then
      mv "$framework.hidden" "$framework"
    fi
  done
}

release_runtime_lock() {
  if [[ -n "$runtime_lock_dir" && -d "$runtime_lock_dir" ]]; then
    rmdir "$runtime_lock_dir" 2>/dev/null || true
  fi
}

cleanup_runtime_state() {
  restore_hidden_qt_runtime
  release_runtime_lock
}

acquire_runtime_lock() {
  local lock_root="$app_path/Contents/.qt-runtime-smoke.lock"
  local waited=0
  while ! mkdir "$lock_root" 2>/dev/null; do
    if ((waited >= runtime_lock_timeout)); then
      echo "error: timed out waiting for Qt runtime smoke lock: $lock_root" >&2
      exit 124
    fi
    sleep 1
    waited=$((waited + 1))
  done
  runtime_lock_dir="$lock_root"
  trap cleanup_runtime_state EXIT
}

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
  if [[ "${launch_with_open:-0}" == "1" ]]; then
    [[ -n "${open_pid:-}" ]] && kill -0 "$open_pid" 2>/dev/null
  else
    jobs -pr | grep -Fxq "$pid"
  fi
}

stop_smoke_process() {
  # OpenToonz handles SIGTERM as a crash signal; bounded smokes use SIGKILL so
  # harness cleanup does not generate misleading crash-handler output.
  if [[ -n "${pid:-}" ]]; then
    kill -9 "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
  if [[ -n "${open_pid:-}" ]]; then
    kill -9 "$open_pid" 2>/dev/null || true
    wait "$open_pid" 2>/dev/null || true
  fi
}

if [[ ! -x "$executable" ]]; then
  echo "error: OpenToonz executable not found: $executable" >&2
  exit 1
fi

if [[ ! -f "$script_path" ]]; then
  echo "error: script smoke fixture not found: $script_path" >&2
  exit 1
fi

if [[ -n "$library_fixture" && ! -f "$library_fixture" ]]; then
  echo "error: script smoke library fixture not found: $library_fixture" >&2
  exit 1
fi

smoke_home="$smoke_root/smoke-home"
mkdir -p "$smoke_home" "$smoke_root/xdg-config" "$smoke_root/xdg-cache"
if [[ ! -d "$smoke_root/stuff" ]]; then
  cp -pR "$repo_root/stuff" "$smoke_root/stuff"
fi
mkdir -p \
  "$smoke_home/.config/OpenToonz" \
  "$smoke_root/stuff/cache" \
  "$smoke_root/stuff/config" \
  "$smoke_root/stuff/fxs" \
  "$smoke_root/stuff/library" \
  "$smoke_root/stuff/profiles" \
  "$smoke_root/stuff/projects" \
  "$smoke_root/stuff/studiopalette"
system_var_path="$smoke_home/.config/OpenToonz/SystemVar.ini"
{
  printf '[General]\n'
  printf 'TOONZROOT=%s\n' "$smoke_root/stuff"
  printf 'TOONZPROFILES=%s\n' "$smoke_root/stuff/profiles"
  printf 'TOONZCONFIG=%s\n' "$smoke_root/stuff/config"
  printf 'TOONZLIBRARY=%s\n' "$smoke_root/stuff/library"
  printf 'TOONZSTUDIOPALETTE=%s\n' "$smoke_root/stuff/studiopalette"
  printf 'TOONZFXPRESETS=%s\n' "$smoke_root/stuff/fxs"
  printf 'TOONZPROJECTS=%s\n' "$smoke_root/stuff/projects"
  printf 'TOONZCACHEROOT=%s\n' "$smoke_root/stuff/cache"
} >"$system_var_path"
if [[ ! -e "$smoke_root/toonz" ]]; then
  ln -s "$repo_root/toonz" "$smoke_root/toonz"
fi
if [[ -n "$library_fixture" ]]; then
  mkdir -p "$smoke_root/stuff/library/scripts"
  cp -p "$library_fixture" "$smoke_root/stuff/library/scripts/$(basename "$library_fixture")"
fi

launch_args=(
  -TOONZROOT "$smoke_root/stuff"
  -TOONZPROFILES "$smoke_root/stuff/profiles"
  -TOONZCONFIG "$smoke_root/stuff/config"
  -TOONZLIBRARY "$smoke_root/stuff/library"
  -TOONZSTUDIOPALETTE "$smoke_root/stuff/studiopalette"
  -TOONZFXPRESETS "$smoke_root/stuff/fxs"
  -TOONZPROJECTS "$smoke_root/stuff/projects"
  "$script_path"
)

launch_with_open=0
if [[ "$(uname -s)" == "Darwin" &&
      "${OPENTOONZ_SCRIPT_USE_QAPPLICATION:-0}" == "1" &&
      "${OPENTOONZ_SCRIPT_SMOKE_DIRECT_EXEC:-0}" != "1" &&
      -x /usr/bin/open ]]; then
  launch_with_open=1
fi

if [[ "$(uname -s)" == "Darwin" && -z "$qt_plugin_path" ]]; then
  qt_core_path="$(otool -L "$executable" 2>/dev/null |
    awk '$1 ~ /qtbase-/ && $1 ~ /\/lib\/QtCore.framework\/Versions\/A\/QtCore/ {print $1; exit}')"
if [[ "$qt_core_path" == /nix/store/*/lib/QtCore.framework/Versions/A/QtCore ]]; then
    qt_plugin_path="${qt_core_path%/lib/QtCore.framework/Versions/A/QtCore}/lib/qt-6/plugins"
  fi
fi
if [[ -n "$qt_plugin_path" ]]; then
  acquire_runtime_lock
fi
if [[ -n "$qt_plugin_path" &&
      -f "$app_path/Contents/Resources/qt.conf" &&
      ! -f "$app_path/Contents/Resources/qt.conf.hidden" ]]; then
  hidden_qt_conf="$app_path/Contents/Resources/qt.conf"
  mv "$hidden_qt_conf" "$hidden_qt_conf.hidden"
fi
if [[ -n "$qt_plugin_path" && -d "$app_path/Contents/Frameworks" ]]; then
  for framework in "$app_path"/Contents/Frameworks/Qt*.framework; do
    if [[ -d "$framework" && ! -e "$framework.hidden" ]]; then
      mv "$framework" "$framework.hidden"
      hidden_qt_frameworks+=("$framework")
    fi
  done
fi

if [[ "$launch_with_open" == "1" ]]; then
  (
    cd "$smoke_root"
    /usr/bin/open -W -n "$app_path" \
      --stdout "$log_path" \
      --stderr "$log_path" \
      --env "HOME=$smoke_home" \
      --env "XDG_CONFIG_HOME=$smoke_root/xdg-config" \
      --env "XDG_CACHE_HOME=$smoke_root/xdg-cache" \
      ${qt_plugin_path:+--env "QT_PLUGIN_PATH=$qt_plugin_path"} \
      ${qt_plugin_path:+--env "QT_QPA_PLATFORM_PLUGIN_PATH=$qt_plugin_path/platforms"} \
      --env "TOONZROOT=$smoke_root/stuff" \
      --env "TOONZPROFILES=$smoke_root/stuff/profiles" \
      --env "TOONZCONFIG=$smoke_root/stuff/config" \
      --env "TOONZLIBRARY=$smoke_root/stuff/library" \
      --env "TOONZSTUDIOPALETTE=$smoke_root/stuff/studiopalette" \
      --env "TOONZFXPRESETS=$smoke_root/stuff/fxs" \
    --env "TOONZPROJECTS=$smoke_root/stuff/projects" \
    --env "TOONZCACHEROOT=$smoke_root/stuff/cache" \
    --env "OPENTOONZ_SCRIPT_USE_QAPPLICATION=1" \
    --env "OPENTOONZ_SCRIPT_WORKING_DIRECTORY=$smoke_root" \
    --args "${launch_args[@]}"
  ) \
    >>"$log_path" 2>&1 &
  open_pid=$!
  pid=""
  for ((attempt = 0; attempt < 100; attempt++)); do
    pid="$(pgrep -n -f "$executable" 2>/dev/null || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      break
    fi
    sleep 0.1
  done
else
  (
    cd "$smoke_root"
  env -u DYLD_FRAMEWORK_PATH \
    -u DYLD_LIBRARY_PATH \
    -u QML2_IMPORT_PATH \
    -u QML_IMPORT_PATH \
    ${qt_plugin_path:+QT_PLUGIN_PATH="$qt_plugin_path"} \
    ${qt_plugin_path:+QT_QPA_PLATFORM_PLUGIN_PATH="$qt_plugin_path/platforms"} \
    HOME="$smoke_home" \
    XDG_CONFIG_HOME="$smoke_root/xdg-config" \
    XDG_CACHE_HOME="$smoke_root/xdg-cache" \
    TOONZROOT="$smoke_root/stuff" \
    TOONZPROFILES="$smoke_root/stuff/profiles" \
    TOONZCONFIG="$smoke_root/stuff/config" \
    TOONZLIBRARY="$smoke_root/stuff/library" \
    TOONZSTUDIOPALETTE="$smoke_root/stuff/studiopalette" \
    TOONZFXPRESETS="$smoke_root/stuff/fxs" \
    TOONZPROJECTS="$smoke_root/stuff/projects" \
    TOONZCACHEROOT="$smoke_root/stuff/cache" \
    OPENTOONZ_SCRIPT_WORKING_DIRECTORY="$smoke_root" \
    "$executable" \
      "${launch_args[@]}"
  ) \
    >"$log_path" 2>&1 &
  pid=$!
fi

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
if [[ "$launch_with_open" == "1" ]]; then
  wait "$open_pid"
else
  wait "$pid"
fi
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
