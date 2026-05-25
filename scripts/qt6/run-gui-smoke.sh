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
  startup|create-scene|open-scene|high-dpi|media-devices|audio-input|audio-recording-wav|audio-playback-wav|camera-formats|audio-output|xsheet-scrub) ;;
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

    if [[ "$smoke_action" == "high-dpi" ]]; then
      if ! high_dpi_window="$(wait_for_app_smoke_status high-dpi)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI high-DPI smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      window_dpr="$(status_value windowDevicePixelRatio || true)"
      screen_dpr="$(status_value screenDevicePixelRatio || true)"
      logical_dpi_x="$(status_value logicalDpiX || true)"
      logical_dpi_y="$(status_value logicalDpiY || true)"
      high_dpi_mode="$(status_value highDpiMode || true)"
      if [[ -z "$window_dpr" || "$window_dpr" == "0" || "$window_dpr" == "0.00" ||
            -z "$screen_dpr" || "$screen_dpr" == "0" || "$screen_dpr" == "0.00" ||
            -z "$logical_dpi_x" || "$logical_dpi_x" == "0" || "$logical_dpi_x" == "0.00" ||
            -z "$logical_dpi_y" || "$logical_dpi_y" == "0" || "$logical_dpi_y" == "0.00" ||
            "$high_dpi_mode" != "qt6-always-on" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI high-DPI smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI high-DPI smoke launched and is still running: pid $pid"
        echo "Window: $high_dpi_window"
        echo "Window DPR: $window_dpr"
        echo "Screen DPR: $screen_dpr"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI high-DPI smoke passed: $high_dpi_window"
      echo "Window DPR: $window_dpr"
      echo "Screen DPR: $screen_dpr"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "media-devices" ]]; then
      if ! media_window="$(wait_for_app_smoke_status media-devices)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI media-devices smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      qt_multimedia_api="$(status_value qtMultimediaApi || true)"
      audio_input_count="$(status_value audioInputCount || true)"
      audio_output_count="$(status_value audioOutputCount || true)"
      video_input_count="$(status_value videoInputCount || true)"
      if [[ "$qt_multimedia_api" != "Qt6" ||
            ! "$audio_input_count" =~ ^[0-9]+$ ||
            ! "$audio_output_count" =~ ^[0-9]+$ ||
            ! "$video_input_count" =~ ^[0-9]+$ ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI media-devices smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI media-devices smoke launched and is still running: pid $pid"
        echo "Window: $media_window"
        echo "Audio inputs: $audio_input_count"
        echo "Audio outputs: $audio_output_count"
        echo "Video inputs: $video_input_count"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI media-devices smoke passed: $media_window"
      echo "Audio inputs: $audio_input_count"
      echo "Audio outputs: $audio_output_count"
      echo "Video inputs: $video_input_count"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "camera-formats" ]]; then
      if ! camera_window="$(wait_for_app_smoke_status camera-formats)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI camera-formats smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      qt_multimedia_api="$(status_value qtMultimediaApi || true)"
      video_input_count="$(status_value videoInputCount || true)"
      default_video_input_available="$(status_value defaultVideoInputAvailable || true)"
      default_video_format_count="$(status_value defaultVideoFormatCount || true)"
      camera_format_probe="$(status_value cameraFormatProbe || true)"
      if [[ "$qt_multimedia_api" != "Qt6" ||
            ! "$video_input_count" =~ ^[0-9]+$ ||
            ! "$default_video_format_count" =~ ^[0-9]+$ ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI camera-formats smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if ((video_input_count == 0)); then
        if [[ "$camera_format_probe" != "no-camera" ]]; then
          stop_smoke_process
          echo "error: Qt 6 GUI camera-formats smoke expected no-camera state" >&2
          sed -n '1,120p' "$status_path" >&2
          sed -n '1,180p' "$log_path" >&2
          exit 1
        fi
      elif [[ "$default_video_input_available" != "true" ||
              "$camera_format_probe" != "ok" ||
              "$default_video_format_count" == "0" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI camera-formats smoke reported no usable default camera formats" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI camera-formats smoke launched and is still running: pid $pid"
        echo "Window: $camera_window"
        echo "Video inputs: $video_input_count"
        echo "Default camera formats: $default_video_format_count"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI camera-formats smoke passed: $camera_window"
      echo "Video inputs: $video_input_count"
      echo "Default camera formats: $default_video_format_count"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-input" ]]; then
      if ! audio_input_window="$(wait_for_app_smoke_status audio-input)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-input smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      qt_multimedia_api="$(status_value qtMultimediaApi || true)"
      audio_input_available="$(status_value audioInputAvailable || true)"
      audio_input_probe="$(status_value audioInputProbe || true)"
      audio_error="$(status_value audioError || true)"
      sample_rate="$(status_value sampleRate || true)"
      bytes_captured="$(status_value bytesCaptured || true)"
      if [[ "$qt_multimedia_api" != "Qt6" ||
            "$audio_input_available" != "true" ||
            "$audio_input_probe" != "ok" ||
            "$audio_error" != "NoError" ||
            ! "$sample_rate" =~ ^[0-9]+$ ||
            ! "$bytes_captured" =~ ^[0-9]+$ ||
            "$sample_rate" == "0" ||
            "$bytes_captured" == "0" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-input smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI audio-input smoke launched and is still running: pid $pid"
        echo "Window: $audio_input_window"
        echo "Sample rate: $sample_rate"
        echo "Bytes captured: $bytes_captured"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI audio-input smoke passed: $audio_input_window"
      echo "Sample rate: $sample_rate"
      echo "Bytes captured: $bytes_captured"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-recording-wav" ]]; then
      if ! audio_recording_window="$(wait_for_app_smoke_status audio-recording-wav)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-recording-wav smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      qt_multimedia_api="$(status_value qtMultimediaApi || true)"
      audio_input_available="$(status_value audioInputAvailable || true)"
      recording_probe="$(status_value audioRecordingWavProbe || true)"
      audio_error="$(status_value audioError || true)"
      sample_rate="$(status_value sampleRate || true)"
      bytes_recorded="$(status_value bytesRecorded || true)"
      wav_file_size="$(status_value wavFileSize || true)"
      wav_load_ok="$(status_value wavLoadOk || true)"
      wav_sample_count="$(status_value wavSampleCount || true)"
      recording_path="$(status_value recordingPath || true)"
      if [[ "$qt_multimedia_api" != "Qt6" ||
            "$audio_input_available" != "true" ||
            "$recording_probe" != "ok" ||
            "$audio_error" != "NoError" ||
            "$wav_load_ok" != "true" ||
            ! "$sample_rate" =~ ^[0-9]+$ ||
            ! "$bytes_recorded" =~ ^[0-9]+$ ||
            ! "$wav_file_size" =~ ^[0-9]+$ ||
            ! "$wav_sample_count" =~ ^[0-9]+$ ||
            "$sample_rate" == "0" ||
            "$bytes_recorded" == "0" ||
            "$wav_file_size" -le 44 ||
            "$wav_sample_count" == "0" ||
            ! -f "$recording_path" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-recording-wav smoke reported unexpected state" >&2
        sed -n '1,140p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI audio-recording-wav smoke launched and is still running: pid $pid"
        echo "Window: $audio_recording_window"
        echo "Sample rate: $sample_rate"
        echo "Bytes recorded: $bytes_recorded"
        echo "WAV sample count: $wav_sample_count"
        echo "Recording: $recording_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI audio-recording-wav smoke passed: $audio_recording_window"
      echo "Sample rate: $sample_rate"
      echo "Bytes recorded: $bytes_recorded"
      echo "WAV sample count: $wav_sample_count"
      echo "Recording: $recording_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-output" ]]; then
      if ! audio_window="$(wait_for_app_smoke_status audio-output)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-output smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      qt_multimedia_api="$(status_value qtMultimediaApi || true)"
      audio_output_available="$(status_value audioOutputAvailable || true)"
      audio_output_probe="$(status_value audioOutputProbe || true)"
      audio_error="$(status_value audioError || true)"
      sample_rate="$(status_value sampleRate || true)"
      bytes_provided="$(status_value bytesProvided || true)"
      if [[ "$qt_multimedia_api" != "Qt6" ||
            "$audio_output_available" != "true" ||
            "$audio_output_probe" != "ok" ||
            "$audio_error" != "NoError" ||
            ! "$sample_rate" =~ ^[0-9]+$ ||
            ! "$bytes_provided" =~ ^[0-9]+$ ||
            "$sample_rate" == "0" ||
            "$bytes_provided" == "0" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-output smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI audio-output smoke launched and is still running: pid $pid"
        echo "Window: $audio_window"
        echo "Sample rate: $sample_rate"
        echo "Bytes provided: $bytes_provided"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI audio-output smoke passed: $audio_window"
      echo "Sample rate: $sample_rate"
      echo "Bytes provided: $bytes_provided"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-playback-wav" ]]; then
      if ! playback_window="$(wait_for_app_smoke_status audio-playback-wav)"; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-playback-wav smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      qt_multimedia_api="$(status_value qtMultimediaApi || true)"
      audio_output_available="$(status_value audioOutputAvailable || true)"
      target_format_supported="$(status_value targetFormatSupported || true)"
      playback_probe="$(status_value audioPlaybackWavProbe || true)"
      output_installed="$(status_value audioOutputInstalled || true)"
      playback_open_ok="$(status_value playbackOpenOk || true)"
      playback_started="$(status_value playbackStarted || true)"
      playback_stopped="$(status_value playbackStoppedAfterStop || true)"
      wav_load_ok="$(status_value wavLoadOk || true)"
      sample_rate="$(status_value sampleRate || true)"
      bytes_written="$(status_value bytesWritten || true)"
      wav_file_size="$(status_value wavFileSize || true)"
      wav_sample_count="$(status_value wavSampleCount || true)"
      playback_path="$(status_value playbackPath || true)"
      if [[ "$qt_multimedia_api" != "Qt6" ||
            "$audio_output_available" != "true" ||
            "$target_format_supported" != "true" ||
            "$playback_probe" != "ok" ||
            "$output_installed" != "true" ||
            "$playback_open_ok" != "true" ||
            "$playback_started" != "true" ||
            "$playback_stopped" != "true" ||
            "$wav_load_ok" != "true" ||
            ! "$sample_rate" =~ ^[0-9]+$ ||
            ! "$bytes_written" =~ ^[0-9]+$ ||
            ! "$wav_file_size" =~ ^[0-9]+$ ||
            ! "$wav_sample_count" =~ ^[0-9]+$ ||
            "$sample_rate" == "0" ||
            "$bytes_written" == "0" ||
            "$wav_file_size" -le 44 ||
            "$wav_sample_count" == "0" ||
            ! -f "$playback_path" ]]; then
        stop_smoke_process
        echo "error: Qt 6 GUI audio-playback-wav smoke reported unexpected state" >&2
        sed -n '1,150p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "Qt 6 GUI audio-playback-wav smoke launched and is still running: pid $pid"
        echo "Window: $playback_window"
        echo "Sample rate: $sample_rate"
        echo "Bytes written: $bytes_written"
        echo "WAV sample count: $wav_sample_count"
        echo "Playback WAV: $playback_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "Qt 6 GUI audio-playback-wav smoke passed: $playback_window"
      echo "Sample rate: $sample_rate"
      echo "Bytes written: $bytes_written"
      echo "WAV sample count: $wav_sample_count"
      echo "Playback WAV: $playback_path"
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
