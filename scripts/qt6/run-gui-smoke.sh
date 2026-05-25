#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
app_path="${OPENTOONZ_APP:-$repo_root/toonz/build/nix-qt6-relwithdebinfo/toonz/OpenToonz.app}"
smoke_root="${OPENTOONZ_GUI_SMOKE_ROOT:-$repo_root/toonz/build/qt6-gui-smoke}"
stable_seconds="${OPENTOONZ_GUI_SMOKE_STABLE_SECONDS:-12}"
timeout_seconds="${OPENTOONZ_GUI_SMOKE_TIMEOUT:-45}"
hold_app="${OPENTOONZ_GUI_SMOKE_HOLD:-0}"
scene_path="${OPENTOONZ_GUI_SMOKE_FILE:-}"
smoke_action="${OPENTOONZ_GUI_SMOKE_ACTION:-}"
scene_name="${OPENTOONZ_GUI_SMOKE_SCENE_NAME:-qt6_gui_smoke_$(date +%Y%m%d%H%M%S)}"

executable="$app_path/Contents/MacOS/OpenToonz"
log_path="$smoke_root/gui-smoke.log"
pid_path="$smoke_root/gui-smoke.pid"
status_path="$smoke_root/gui-smoke.status"

is_smoke_process_running() {
  jobs -pr | grep -Fxq "$pid"
}

stop_smoke_process() {
  # OpenToonz handles SIGTERM as a crash signal; bounded smokes use SIGKILL so
  # harness cleanup does not generate misleading crash-handler output.
  kill -9 "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}

has_fatal_startup_output() {
  grep -Eiq \
    'could not load the Qt platform plugin|no Qt platform plugin could be initialized|Cannot load library .*platforms/libqcocoa|Library not loaded: .*Qt|duplicate class .* is implemented in both|Abort trap|Segmentation fault' \
    "$log_path"
}

status_value() {
  local key="$1"
  if [[ ! -f "$status_path" ]]; then
    return 1
  fi
  awk -F= -v key="$key" '$1 == key { sub(/^[^=]*=/, ""); print; found=1; exit } END { exit found ? 0 : 1 }' "$status_path"
}

wait_for_app_smoke_status() {
  local expected_action="$1"
  local action status window

  for _ in {1..100}; do
    if [[ -f "$status_path" ]]; then
      action="$(status_value action || true)"
      status="$(status_value status || true)"
      if [[ "$action" == "$expected_action" && "$status" == "ok" ]]; then
        window="$(status_value window || true)"
        if [[ -n "$window" ]]; then
          echo "$window"
        else
          echo "app-side $expected_action smoke"
        fi
        return 0
      fi
      if [[ "$action" == "$expected_action" && "$status" == "error" ]]; then
        echo "error: app-side Qt 6 GUI $expected_action smoke failed" >&2
        sed -n '1,80p' "$status_path" >&2
        return 2
      fi
    fi
    sleep 0.1
  done

  return 1
}

drive_scene_create_with_system_events() {
  if ! command -v osascript >/dev/null 2>&1; then
    echo "error: scene-create GUI smoke requires osascript on macOS" >&2
    return 1
  fi

  osascript - "$scene_name" <<'APPLESCRIPT'
on run argv
  set sceneName to item 1 of argv
  tell application "System Events"
    tell process "OpenToonz"
      repeat 100 times
        if exists window "OpenToonz crashed!" then error "OpenToonz crash dialog appeared before scene creation"
        if exists window "OpenToonz Startup" then exit repeat
        delay 0.1
      end repeat

      if not (exists window "OpenToonz Startup") then error "OpenToonz Startup window did not appear"

      tell window "OpenToonz Startup"
        click radio button "Create a New Scene" of tab group 1
        delay 0.2
        set value of text field 1 of group 2 to sceneName
        click button "Create Scene" of group 2
      end tell

      repeat 100 times
        if exists window "OpenToonz crashed!" then error "OpenToonz crash dialog appeared during scene creation"
        set windowNames to name of every window
        repeat with windowName in windowNames
          set windowTitle to windowName as text
          if windowTitle starts with (sceneName & " ") then return windowTitle
        end repeat
        delay 0.1
      end repeat

      error "Created scene window did not appear"
    end tell
  end tell
end run
APPLESCRIPT
}

drive_scene_create_smoke() {
  local created_window status_rc

  if created_window="$(wait_for_app_smoke_status create-scene)"; then
    echo "$created_window"
    return 0
  fi
  status_rc=$?
  if ((status_rc == 2)); then
    return 1
  fi

  drive_scene_create_with_system_events
}

verify_scene_open_with_system_events() {
  local expected_scene_name="$1"

  if ! command -v osascript >/dev/null 2>&1; then
    echo "error: scene-open GUI smoke requires osascript on macOS" >&2
    return 1
  fi

  osascript - "$expected_scene_name" <<'APPLESCRIPT'
on run argv
  set sceneName to item 1 of argv
  tell application "System Events"
    tell process "OpenToonz"
      repeat 100 times
        if exists window "OpenToonz crashed!" then error "OpenToonz crash dialog appeared while opening scene"
        set windowNames to name of every window
        repeat with windowName in windowNames
          set windowTitle to windowName as text
          if windowTitle starts with (sceneName & " ") then return windowTitle
        end repeat
        delay 0.1
      end repeat

      error "Scene window did not appear"
    end tell
  end tell
end run
APPLESCRIPT
}

verify_scene_open_smoke() {
  local expected_scene_name="$1"
  local opened_window status_rc

  if opened_window="$(wait_for_app_smoke_status open-scene)"; then
    echo "$opened_window"
    return 0
  fi
  status_rc=$?
  if ((status_rc == 2)); then
    return 1
  fi

  verify_scene_open_with_system_events "$expected_scene_name"
}

if [[ ! -x "$executable" ]]; then
  echo "error: OpenToonz executable not found: $executable" >&2
  exit 1
fi

if [[ -z "$smoke_action" ]]; then
  if [[ -n "$scene_path" ]]; then
    smoke_action="open-scene"
  else
    smoke_action="startup"
  fi
fi

case "$smoke_action" in
  startup|create-scene|open-scene|xsheet-scrub) ;;
  *)
    echo "error: unsupported OPENTOONZ_GUI_SMOKE_ACTION: $smoke_action" >&2
    exit 1
    ;;
esac

if [[ -n "$scene_path" && "$scene_path" != /* ]]; then
  scene_path="$repo_root/$scene_path"
fi

if [[ -n "$scene_path" && ! -f "$scene_path" ]]; then
  echo "error: GUI smoke scene file not found: $scene_path" >&2
  exit 1
fi

mkdir -p "$smoke_root/home" "$smoke_root/xdg-config" "$smoke_root/xdg-cache"
if [[ ! -d "$smoke_root/stuff" ]]; then
  cp -pR "$repo_root/stuff" "$smoke_root/stuff"
fi

rm -f "$log_path" "$pid_path" "$status_path"

launch_args=(-TOONZROOT "$smoke_root/stuff")
if [[ -n "$scene_path" ]]; then
  launch_args+=("$scene_path")
fi

HOME="$smoke_root/home" \
XDG_CONFIG_HOME="$smoke_root/xdg-config" \
XDG_CACHE_HOME="$smoke_root/xdg-cache" \
OPENTOONZ_GUI_SMOKE_ACTION="$smoke_action" \
OPENTOONZ_GUI_SMOKE_SCENE_NAME="$scene_name" \
OPENTOONZ_GUI_SMOKE_STATUS_FILE="$status_path" \
"$executable" "${launch_args[@]}" \
  >"$log_path" 2>&1 &
pid=$!
echo "$pid" >"$pid_path"

elapsed=0
while is_smoke_process_running; do
  if has_fatal_startup_output; then
    stop_smoke_process
    echo "error: Qt 6 GUI smoke saw fatal startup output" >&2
    sed -n '1,180p' "$log_path" >&2
    exit 1
  fi

  if ((elapsed >= stable_seconds)); then
    if [[ "$smoke_action" == "create-scene" ]]; then
      if ! created_window="$(drive_scene_create_smoke)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI scene-create smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      created_scene_path="$smoke_root/stuff/sandbox/scenes/${scene_name}.tnz"
      if [[ ! -f "$created_scene_path" ]]; then
        stop_smoke_process
        echo "error: expected scene file was not created: $created_scene_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI scene-create smoke launched and is still running: pid $pid"
        echo "Window: $created_window"
        echo "Scene: $created_scene_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI scene-create smoke passed: $created_window"
      echo "Scene: $created_scene_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "open-scene" ]]; then
      if [[ -z "$scene_path" ]]; then
        stop_smoke_process
        echo "error: open-scene GUI smoke requires OPENTOONZ_GUI_SMOKE_FILE" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      scene_title="$(basename "$scene_path")"
      scene_title="${scene_title%.*}"
      if ! opened_window="$(verify_scene_open_smoke "$scene_title")"; then
        stop_smoke_process
        echo "error: Qt 6 GUI scene-open smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI scene-open smoke launched and is still running: pid $pid"
        echo "Window: $opened_window"
        echo "Scene: $scene_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI scene-open smoke passed: $opened_window"
      echo "Scene: $scene_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "xsheet-scrub" ]]; then
      if ! xsheet_window="$(wait_for_app_smoke_status xsheet-scrub)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI xsheet-scrub smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      created_scene_path="$smoke_root/stuff/sandbox/scenes/${scene_name}.tnz"
      frame="$(status_value frame || true)"
      column="$(status_value column || true)"
      frame_count="$(status_value frameCount || true)"
      column_count="$(status_value columnCount || true)"
      raster_frames="$(status_value rasterFrames || true)"
      vector_frames="$(status_value vectorFrames || true)"
      if [[ ! -f "$created_scene_path" ||
            "$frame" != "2" ||
            "$column" != "1" ||
            "$frame_count" != "3" ||
            "$column_count" != "2" ||
            "$raster_frames" != "1" ||
            "$vector_frames" != "1" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI xsheet-scrub smoke reported unexpected state" >&2
        sed -n '1,80p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI xsheet-scrub smoke launched and is still running: pid $pid"
        echo "Window: $xsheet_window"
        echo "Scene: $created_scene_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI xsheet-scrub smoke passed: $xsheet_window"
      echo "Scene: $created_scene_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$hold_app" == "1" ]]; then
      echo "Qt 6 GUI smoke launched and is still running after ${stable_seconds}s: pid $pid"
      echo "Log: $log_path"
      wait "$pid"
      exit $?
    fi

    stop_smoke_process
    echo "Qt 6 GUI smoke passed: app stayed running for ${stable_seconds}s"
    echo "Log: $log_path"
    exit 0
  fi

  if ((elapsed >= timeout_seconds)); then
    stop_smoke_process
    echo "error: Qt 6 GUI smoke timed out after ${timeout_seconds}s" >&2
    sed -n '1,180p' "$log_path" >&2
    exit 124
  fi

  sleep 1
  elapsed=$((elapsed + 1))
done

set +e
wait "$pid"
status=$?
set -e

echo "error: Qt 6 GUI smoke exited early with status $status" >&2
sed -n '1,180p' "$log_path" >&2
exit "$status"
