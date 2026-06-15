#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
app_path="${OPENTOONZ_APP:-$repo_root/toonz/build/nix-qt6-relwithdebinfo/toonz/OpenToonz.app}"
smoke_root="${OPENTOONZ_GUI_SMOKE_ROOT:-$repo_root/toonz/build/qt6-gui-smoke}"
smoke_label="${OPENTOONZ_GUI_SMOKE_LABEL:-Qt 6 GUI}"
stable_seconds="${OPENTOONZ_GUI_SMOKE_STABLE_SECONDS:-12}"
timeout_seconds="${OPENTOONZ_GUI_SMOKE_TIMEOUT:-45}"
runtime_lock_timeout="${OPENTOONZ_GUI_SMOKE_LOCK_TIMEOUT:-$((timeout_seconds * 2))}"
hold_app="${OPENTOONZ_GUI_SMOKE_HOLD:-0}"
scene_path="${OPENTOONZ_GUI_SMOKE_FILE:-}"
smoke_action="${OPENTOONZ_GUI_SMOKE_ACTION:-}"
scene_name="${OPENTOONZ_GUI_SMOKE_SCENE_NAME:-qt6_gui_smoke_$(date +%Y%m%d%H%M%S)}"

if [[ "$app_path" != /* ]]; then
  app_path="$repo_root/$app_path"
fi
if [[ "$smoke_root" != /* ]]; then
  smoke_root="$repo_root/$smoke_root"
fi

executable="$app_path/Contents/MacOS/OpenToonz"
log_path="$smoke_root/gui-smoke.log"
pid_path="$smoke_root/gui-smoke.pid"
status_path="$smoke_root/gui-smoke.status"
qt_plugin_path="${OPENTOONZ_GUI_SMOKE_QT_PLUGIN_PATH:-}"
hidden_qt_conf=""
hidden_qt_frameworks=()
runtime_lock_dir=""

restore_hidden_qt_conf() {
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
  restore_hidden_qt_conf
  release_runtime_lock
}

acquire_runtime_lock() {
  local lock_parent="$repo_root/toonz/build/.qt-runtime-smoke-locks"
  local lock_name
  lock_name="$(printf '%s' "$app_path" | tr '/ :' '___')"
  local lock_root="$lock_parent/$lock_name.lock"
  local waited=0
  mkdir -p "$lock_parent"
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

is_smoke_process_running() {
  if [[ "${launch_with_open:-0}" == "1" ]]; then
    [[ -n "${pid:-}" ]] && kill -0 "$pid" 2>/dev/null
  else
    jobs -pr | grep -Fxq "$pid"
  fi
}

stop_smoke_process() {
  # OpenToonz handles SIGTERM as a crash signal; bounded smokes use SIGKILL so
  # harness cleanup does not generate misleading crash-handler output.
  kill -9 "$pid" 2>/dev/null || true
  if [[ -n "${open_pid:-}" ]]; then
    kill -9 "$open_pid" 2>/dev/null || true
    wait "$open_pid" 2>/dev/null || true
  fi
  wait "$pid" 2>/dev/null || true
}

wait_for_smoke_process_exit() {
  if [[ "${launch_with_open:-0}" == "1" && -n "${open_pid:-}" ]]; then
    wait "$open_pid"
  else
    wait "$pid"
  fi
}

has_fatal_startup_output() {
  [[ -f "$log_path" ]] || return 1
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
  local max_attempts="${2:-100}"
  local action status window

  for ((attempt = 0; attempt < max_attempts; attempt++)); do
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
        echo "error: app-side $smoke_label $expected_action smoke failed" >&2
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

drive_viewer_raster_brush_with_cg_events() {
  if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: CGEvent viewer brush smoke is only supported on macOS" >&2
    return 1
  fi

  local points swift_bin swift_driver swift_output bundle_id
  points="$(status_value systemMousePoints || true)"
  if [[ -z "$points" ]]; then
    echo "error: app-side CGEvent smoke did not publish systemMousePoints" >&2
    return 1
  fi

  swift_bin="$(xcrun --find swift 2>/dev/null || command -v swift || true)"
  if [[ -z "$swift_bin" ]]; then
    echo "error: CGEvent viewer brush smoke requires swift from Xcode command line tools" >&2
    return 1
  fi

  bundle_id="$(plutil -extract CFBundleIdentifier raw "$app_path/Contents/Info.plist" 2>/dev/null || true)"

  if [[ -n "$bundle_id" && -x /usr/bin/open ]]; then
    /usr/bin/open -b "$bundle_id" >/dev/null 2>&1 || true
    sleep 0.4
  fi

  if command -v osascript >/dev/null 2>&1; then
    if [[ -n "$bundle_id" ]]; then
      osascript - "$bundle_id" <<'APPLESCRIPT' >/dev/null 2>&1 || true
on run argv
  tell application id (item 1 of argv) to activate
end run
APPLESCRIPT
      sleep 0.2
    fi

    osascript - "$pid" <<'APPLESCRIPT' >/dev/null 2>&1 || true
on run argv
  set targetPid to item 1 of argv as integer
  tell application "System Events"
    repeat 30 times
      if exists (first process whose unix id is targetPid) then exit repeat
      delay 0.1
    end repeat
    set frontmost of first process whose unix id is targetPid to true
  end tell
end run
APPLESCRIPT
    sleep 0.2
  fi

  swift_driver="$smoke_root/post_mouse_drag.swift"
  cat >"$swift_driver" <<'SWIFT'
import AppKit
import ApplicationServices
import CoreGraphics
import Foundation

func fail(_ message: String, _ code: Int32) -> Never {
  fputs(message + "\n", stderr)
  exit(code)
}

if CommandLine.arguments.count < 3 {
  fail("missing point list or target pid", 2)
}

let points = CommandLine.arguments[1].split(separator: "|").compactMap { item -> CGPoint? in
  let pair = item.split(separator: ",")
  guard pair.count == 2,
        let x = Double(pair[0]),
        let y = Double(pair[1]) else {
    return nil
  }
  return CGPoint(x: x, y: y)
}

guard points.count >= 2 else {
  fail("point list must contain at least two points", 3)
}

guard let pidValue = Int32(CommandLine.arguments[2]) else {
  fail("target pid is not an integer", 4)
}

guard let targetApp = NSRunningApplication(processIdentifier: pid_t(pidValue)) else {
  fail("could not resolve target application", 5)
}

if #available(macOS 10.15, *) {
  let canPost = CGPreflightPostEventAccess()
  fputs("cgPostEventAccess=\(canPost)\n", stderr)
  if !canPost {
    fail("CGEvent post access is not trusted", 6)
  }
}

let wasActive = targetApp.isActive
let axTrusted = AXIsProcessTrusted()
let targetElement = AXUIElementCreateApplication(pid_t(pidValue))
let axRaiseResult = AXUIElementPerformAction(targetElement,
                                             kAXRaiseAction as CFString)
let axFrontResult = AXUIElementSetAttributeValue(
  targetElement, kAXFrontmostAttribute as CFString, kCFBooleanTrue)
let activationRequested = targetApp.activate(options: [.activateAllWindows])
RunLoop.current.run(until: Date().addingTimeInterval(0.75))
fputs("targetAppActiveBefore=\(wasActive)\n", stderr)
fputs("axProcessTrusted=\(axTrusted)\n", stderr)
fputs("axRaiseResult=\(axRaiseResult.rawValue)\n", stderr)
fputs("axFrontmostResult=\(axFrontResult.rawValue)\n", stderr)
fputs("targetAppActivationRequested=\(activationRequested)\n", stderr)
fputs("targetAppActiveAfter=\(targetApp.isActive)\n", stderr)
if let frontmostApp = NSWorkspace.shared.frontmostApplication {
  let bundleID = frontmostApp.bundleIdentifier ?? "unknown"
  fputs("frontmostAppPID=\(frontmostApp.processIdentifier)\n", stderr)
  fputs("frontmostAppBundleID=\(bundleID)\n", stderr)
}

guard let source = CGEventSource(stateID: .hidSystemState) else {
  fail("could not create CGEventSource", 7)
}

let backingScale = NSScreen.screens.first(where: { $0.frame.contains(points[0]) })?.backingScaleFactor ??
                   NSScreen.main?.backingScaleFactor ??
                   1.0
fputs("cgEventBackingScale=\(backingScale)\n", stderr)

func windowContains(_ bounds: CGRect, _ point: CGPoint) -> Bool {
  return point.x >= bounds.minX && point.x <= bounds.maxX &&
         point.y >= bounds.minY && point.y <= bounds.maxY
}

func targetWindowDiagnostics(_ pointSets: [(String, [CGPoint])]) {
  guard let windowList = CGWindowListCopyWindowInfo(
      [.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID)
      as? [[String: Any]] else {
    fputs("targetWindowProbe=unavailable\n", stderr)
    return
  }

  let targetWindows = windowList.filter { info in
    guard let ownerPID = info[kCGWindowOwnerPID as String] as? Int else {
      return false
    }
    return ownerPID == Int(pidValue)
  }
  fputs("targetWindowCount=\(targetWindows.count)\n", stderr)

  for (index, info) in targetWindows.prefix(5).enumerated() {
    guard let boundsDict = info[kCGWindowBounds as String] as? NSDictionary,
          let bounds = CGRect(dictionaryRepresentation: boundsDict) else {
      continue
    }
    let number = info[kCGWindowNumber as String] as? Int ?? -1
    let layer = info[kCGWindowLayer as String] as? Int ?? -1
    var containsParts: [String] = []
    for (label, strokePoints) in pointSets {
      guard let first = strokePoints.first, let last = strokePoints.last else {
        continue
      }
      containsParts.append(
        "\(label):first=\(windowContains(bounds, first)),last=\(windowContains(bounds, last))")
    }
    let containsText = containsParts.joined(separator: ";")
    fputs(
      "targetWindow[\(index)]=number=\(number),layer=\(layer),bounds=\(bounds.origin.x),\(bounds.origin.y),\(bounds.size.width),\(bounds.size.height),contains=\(containsText)\n",
      stderr)
  }
}

func post(_ type: CGEventType, at point: CGPoint, hidTap: Bool) {
  guard let event = CGEvent(mouseEventSource: source,
                            mouseType: type,
                            mouseCursorPosition: point,
                            mouseButton: .left) else {
    fail("could not create CGEvent", 8)
  }
  event.setIntegerValueField(.mouseEventClickState, value: 1)
  event.postToPid(pid_t(pidValue))
  if hidTap {
    event.post(tap: .cghidEventTap)
  }
  usleep(70000)
}

func postStroke(_ strokePoints: [CGPoint], label: String, hidTap: Bool) {
  guard let firstPoint = strokePoints.first, let lastPoint = strokePoints.last else {
    return
  }
  fputs("postedPointSet=\(label),hidTap=\(hidTap),first=\(firstPoint.x),\(firstPoint.y),last=\(lastPoint.x),\(lastPoint.y)\n", stderr)
  post(.mouseMoved, at: firstPoint, hidTap: hidTap)
  post(.leftMouseDown, at: firstPoint, hidTap: hidTap)
  post(.leftMouseUp, at: firstPoint, hidTap: hidTap)
  RunLoop.current.run(until: Date().addingTimeInterval(0.45))
  fputs("targetAppActiveAfterPrimeClick[\(label)]=\(targetApp.isActive)\n", stderr)

  post(.mouseMoved, at: firstPoint, hidTap: hidTap)
  post(.leftMouseDown, at: firstPoint, hidTap: hidTap)
  for point in strokePoints.dropFirst() {
    post(.leftMouseDragged, at: point, hidTap: hidTap)
  }
  post(.leftMouseUp, at: lastPoint, hidTap: hidTap)
  RunLoop.current.run(until: Date().addingTimeInterval(0.45))
}

var diagnosticPointSets: [(String, [CGPoint])] = [("qt-global", points)]
if backingScale != 1.0 {
  diagnosticPointSets.append(("backing-scaled", points.map {
    CGPoint(x: $0.x * backingScale, y: $0.y * backingScale)
  }))
}
targetWindowDiagnostics(diagnosticPointSets)

postStroke(points, label: "qt-global", hidTap: true)
if backingScale != 1.0 {
  let scaledPoints = points.map { CGPoint(x: $0.x * backingScale,
                                          y: $0.y * backingScale) }
  postStroke(scaledPoints, label: "backing-scaled", hidTap: false)
}
usleep(150000)
SWIFT

  if ! swift_output="$("$swift_bin" "$swift_driver" "$points" "$pid" 2>&1)"; then
    printf '%s\n' "$swift_output" >&2
    return 1
  fi
  printf '%s\n' "$swift_output"
  echo "$smoke_label viewer-raster-brush-system-events CGEvent driver posted: $points"

  if command -v osascript >/dev/null 2>&1; then
    osascript - "$points" "$pid" <<'APPLESCRIPT' || \
      echo "warning: System Events click fallback failed" >&2
on splitText(theText, delimiter)
  set oldDelimiters to AppleScript's text item delimiters
  set AppleScript's text item delimiters to delimiter
  set parts to text items of theText
  set AppleScript's text item delimiters to oldDelimiters
  return parts
end splitText

on run argv
  set pointList to item 1 of argv
  set targetPid to item 2 of argv as integer
  tell application "System Events"
    repeat 30 times
      if exists (first process whose unix id is targetPid) then exit repeat
      delay 0.1
    end repeat
    set frontmost of first process whose unix id is targetPid to true
    delay 0.2

    set pointItems to my splitText(pointList, "|")
    repeat with pointItem in pointItems
      set pair to my splitText(pointItem as text, ",")
      set pointX to item 1 of pair as integer
      set pointY to item 2 of pair as integer
      click at {pointX, pointY}
      delay 0.08
    end repeat
  end tell
end run
APPLESCRIPT
  fi
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
  startup|create-scene|open-scene|high-dpi|script-console-view|preview-render-output|fx-preview-render-output|fx-preview-subcamera-render-output|fx-preview-flipbook-controls|fx-preview-save-previewed-frames|fx-preview-subcamera-save-previewed-frames|final-render-output|final-render-background-output|final-render-sequence-output|final-render-composite-output|final-render-vector-output|final-render-toonz-raster-output|final-render-fx-output|media-devices|audio-input|audio-recording-wav|audio-playback-wav|camera-formats|audio-output|viewer-render|viewer-vector-render|viewer-zoom-pan|viewer-onion-skin|viewer-onion-skin-rowarea|viewer-onion-skin-rowarea-drag|viewer-onion-skin-fixed-marker-drag|viewer-onion-skin-shift-trace|viewer-onion-skin-context-menu|viewer-onion-skin-custom-colors|viewer-onion-skin-orientations|viewer-camera-overlay|viewer-safe-area-field-guide|viewer-safe-area-presets|viewer-safe-area-custom-file|viewer-field-guide-settings|viewer-ruler-guide|viewer-ruler-guide-events|viewer-ruler-guide-variants|viewer-ruler-guide-lines|viewer-ruler-guide-styles|viewer-ruler-ticks|viewer-animate-tool-overlay|viewer-animate-tool-drag|viewer-animate-tool-mouse-events|viewer-animate-tool-undo-redo|viewer-animate-tool-modifiers|viewer-animate-tool-handles|viewer-animate-tool-handle-variants|viewer-animate-tool-axis-drags|viewer-animate-tool-cursors|viewer-selection-tool-vector-handles|viewer-selection-tool-vector-handle-variants|viewer-selection-tool-vector-center-thickness-deform|viewer-selection-tool-vector-mode-variants|viewer-selection-tool-raster-handles|viewer-selection-tool-raster-mode-variants|viewer-vector-brush|viewer-raster-brush|viewer-raster-brush-mouse-events|viewer-raster-brush-tablet-events|viewer-raster-brush-system-events|xsheet-scrub) ;;
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

launch_with_open=0
if [[ "$(uname -s)" == "Darwin" &&
      "${OPENTOONZ_GUI_SMOKE_DIRECT_EXEC:-0}" != "1" &&
      -x /usr/bin/open ]]; then
  launch_with_open=1
fi

if [[ -z "$qt_plugin_path" ]]; then
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
  /usr/bin/open -W -n "$app_path" \
    --stdout "$log_path" \
    --stderr "$log_path" \
    --env "HOME=$smoke_root/home" \
    --env "XDG_CONFIG_HOME=$smoke_root/xdg-config" \
    --env "XDG_CACHE_HOME=$smoke_root/xdg-cache" \
    ${qt_plugin_path:+--env "QT_PLUGIN_PATH=$qt_plugin_path"} \
    ${qt_plugin_path:+--env "QT_QPA_PLATFORM_PLUGIN_PATH=$qt_plugin_path/platforms"} \
    --env "OPENTOONZ_GUI_SMOKE_ACTION=$smoke_action" \
    --env "OPENTOONZ_GUI_SMOKE_SCENE_NAME=$scene_name" \
    --env "OPENTOONZ_GUI_SMOKE_STATUS_FILE=$status_path" \
    --args "${launch_args[@]}" \
    >/dev/null 2>&1 &
  open_pid=$!
  pid=""
  for ((attempt = 0; attempt < 100; attempt++)); do
    pid="$(pgrep -n -f "$executable" 2>/dev/null || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      break
    fi
    sleep 0.1
  done
  if [[ -z "$pid" ]]; then
    kill -9 "$open_pid" 2>/dev/null || true
    echo "error: failed to resolve OpenToonz pid after LaunchServices launch" >&2
    exit 1
  fi
else
  env -u DYLD_FRAMEWORK_PATH \
    -u DYLD_LIBRARY_PATH \
    -u QML2_IMPORT_PATH \
    -u QML_IMPORT_PATH \
    ${qt_plugin_path:+QT_PLUGIN_PATH="$qt_plugin_path"} \
    ${qt_plugin_path:+QT_QPA_PLATFORM_PLUGIN_PATH="$qt_plugin_path/platforms"} \
    HOME="$smoke_root/home" \
    XDG_CONFIG_HOME="$smoke_root/xdg-config" \
    XDG_CACHE_HOME="$smoke_root/xdg-cache" \
    OPENTOONZ_GUI_SMOKE_ACTION="$smoke_action" \
    OPENTOONZ_GUI_SMOKE_SCENE_NAME="$scene_name" \
    OPENTOONZ_GUI_SMOKE_STATUS_FILE="$status_path" \
    "$executable" "${launch_args[@]}" \
    >"$log_path" 2>&1 &
  pid=$!
fi
echo "$pid" >"$pid_path"

elapsed=0
system_mouse_driven=0
while is_smoke_process_running; do
  if has_fatal_startup_output; then
    stop_smoke_process
    echo "error: $smoke_label smoke saw fatal startup output" >&2
    sed -n '1,180p' "$log_path" >&2
    exit 1
  fi

  if [[ "$smoke_action" == "viewer-raster-brush-system-events" &&
        "$system_mouse_driven" == "0" ]]; then
    current_action="$(status_value action || true)"
    current_status="$(status_value status || true)"
    if [[ "$current_action" == "$smoke_action" && "$current_status" == "ready" ]]; then
      if ! drive_viewer_raster_brush_with_cg_events; then
        stop_smoke_process
        echo "error: $smoke_label ${smoke_action} CGEvent driver failed" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi
      system_mouse_driven=1
    elif [[ "$current_action" == "$smoke_action" && "$current_status" == "error" ]]; then
      stop_smoke_process
      echo "error: app-side $smoke_label $smoke_action smoke failed" >&2
      sed -n '1,120p' "$status_path" >&2
      sed -n '1,180p' "$log_path" >&2
      exit 1
    fi
  fi

  if ((elapsed >= stable_seconds)); then
    if [[ "$smoke_action" == "create-scene" ]]; then
      if ! created_window="$(drive_scene_create_smoke)"; then
        stop_smoke_process
        echo "error: $smoke_label scene-create smoke failed" >&2
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
        echo "$smoke_label scene-create smoke launched and is still running: pid $pid"
        echo "Window: $created_window"
        echo "Scene: $created_scene_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label scene-create smoke passed: $created_window"
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
        echo "error: $smoke_label scene-open smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label scene-open smoke launched and is still running: pid $pid"
        echo "Window: $opened_window"
        echo "Scene: $scene_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label scene-open smoke passed: $opened_window"
      echo "Scene: $scene_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "high-dpi" ]]; then
      if ! high_dpi_window="$(wait_for_app_smoke_status high-dpi)"; then
        stop_smoke_process
        echo "error: $smoke_label high-DPI smoke failed" >&2
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
        echo "error: $smoke_label high-DPI smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label high-DPI smoke launched and is still running: pid $pid"
        echo "Window: $high_dpi_window"
        echo "Window DPR: $window_dpr"
        echo "Screen DPR: $screen_dpr"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label high-DPI smoke passed: $high_dpi_window"
      echo "Window DPR: $window_dpr"
      echo "Screen DPR: $screen_dpr"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "media-devices" ]]; then
      if ! media_window="$(wait_for_app_smoke_status media-devices)"; then
        stop_smoke_process
        echo "error: $smoke_label media-devices smoke failed" >&2
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
        echo "error: $smoke_label media-devices smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label media-devices smoke launched and is still running: pid $pid"
        echo "Window: $media_window"
        echo "Audio inputs: $audio_input_count"
        echo "Audio outputs: $audio_output_count"
        echo "Video inputs: $video_input_count"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label media-devices smoke passed: $media_window"
      echo "Audio inputs: $audio_input_count"
      echo "Audio outputs: $audio_output_count"
      echo "Video inputs: $video_input_count"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "camera-formats" ]]; then
      if ! camera_window="$(wait_for_app_smoke_status camera-formats)"; then
        stop_smoke_process
        echo "error: $smoke_label camera-formats smoke failed" >&2
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
        echo "error: $smoke_label camera-formats smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if ((video_input_count == 0)); then
        if [[ "$camera_format_probe" != "no-camera" ]]; then
          stop_smoke_process
          echo "error: $smoke_label camera-formats smoke expected no-camera state" >&2
          sed -n '1,120p' "$status_path" >&2
          sed -n '1,180p' "$log_path" >&2
          exit 1
        fi
      elif [[ "$default_video_input_available" != "true" ||
              "$camera_format_probe" != "ok" ||
              "$default_video_format_count" == "0" ]]; then
        stop_smoke_process
        echo "error: $smoke_label camera-formats smoke reported no usable default camera formats" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label camera-formats smoke launched and is still running: pid $pid"
        echo "Window: $camera_window"
        echo "Video inputs: $video_input_count"
        echo "Default camera formats: $default_video_format_count"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label camera-formats smoke passed: $camera_window"
      echo "Video inputs: $video_input_count"
      echo "Default camera formats: $default_video_format_count"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-input" ]]; then
      audio_input_window=""
      status_error_path="$smoke_root/audio-input-status.err"
      rm -f "$status_error_path"
      status_rc=0
      audio_input_window="$(wait_for_app_smoke_status audio-input 300 2>"$status_error_path")" ||
        status_rc=$?
      if ((status_rc != 0)); then
        audio_input_probe="$(status_value audioInputProbe || true)"
        if ((status_rc == 2)) &&
           [[ "$audio_input_probe" == "no-device" ||
              "$audio_input_probe" == "timeout" ]]; then
          stop_smoke_process
          echo "$smoke_label audio-input smoke skipped: $audio_input_probe"
          echo "Log: $log_path"
          exit 0
        fi
        stop_smoke_process
        echo "error: $smoke_label audio-input smoke failed" >&2
        sed -n '1,80p' "$status_error_path" >&2
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
        echo "error: $smoke_label audio-input smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label audio-input smoke launched and is still running: pid $pid"
        echo "Window: $audio_input_window"
        echo "Sample rate: $sample_rate"
        echo "Bytes captured: $bytes_captured"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label audio-input smoke passed: $audio_input_window"
      echo "Sample rate: $sample_rate"
      echo "Bytes captured: $bytes_captured"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-recording-wav" ]]; then
      audio_recording_window=""
      status_error_path="$smoke_root/audio-recording-wav-status.err"
      rm -f "$status_error_path"
      status_rc=0
      audio_recording_window="$(wait_for_app_smoke_status audio-recording-wav 300 2>"$status_error_path")" ||
        status_rc=$?
      if ((status_rc != 0)); then
        recording_probe="$(status_value audioRecordingWavProbe || true)"
        if ((status_rc == 2)) &&
           [[ "$recording_probe" == "no-device" ||
              "$recording_probe" == "timeout" ||
              "$recording_probe" == "writer-start-failed" ]]; then
          stop_smoke_process
          echo "$smoke_label audio-recording-wav smoke skipped: $recording_probe"
          echo "Log: $log_path"
          exit 0
        fi
        stop_smoke_process
        echo "error: $smoke_label audio-recording-wav smoke failed" >&2
        sed -n '1,80p' "$status_error_path" >&2
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
        echo "error: $smoke_label audio-recording-wav smoke reported unexpected state" >&2
        sed -n '1,140p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label audio-recording-wav smoke launched and is still running: pid $pid"
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
      echo "$smoke_label audio-recording-wav smoke passed: $audio_recording_window"
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
        echo "error: $smoke_label audio-output smoke failed" >&2
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
        echo "error: $smoke_label audio-output smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label audio-output smoke launched and is still running: pid $pid"
        echo "Window: $audio_window"
        echo "Sample rate: $sample_rate"
        echo "Bytes provided: $bytes_provided"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label audio-output smoke passed: $audio_window"
      echo "Sample rate: $sample_rate"
      echo "Bytes provided: $bytes_provided"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "script-console-view" ]]; then
      if ! script_console_window="$(wait_for_app_smoke_status script-console-view)"; then
        stop_smoke_process
        echo "error: $smoke_label script-console-view smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      script_console_probe="$(status_value scriptConsoleViewProbe || true)"
      script_console_output="$(status_value scriptConsoleOutput || true)"
      script_console_history="$(status_value scriptConsoleHistoryLine || true)"
      script_console_run_script_ready="$(status_value scriptConsoleRunScriptReady || true)"
      visible_flipbook_delta="$(status_value visibleFlipbookDelta || true)"
      if [[ "$script_console_probe" != "ok" ||
            "$script_console_run_script_ready" != "yes" ||
            ! "$visible_flipbook_delta" =~ ^-?[0-9]+$ ||
            "$script_console_output" != *qt-script-console-view* ||
            "$script_console_output" != *qt-script-console-repeat* ||
            "$script_console_output" != *qt-script-console-run-child* ||
            "$script_console_output" != *qt-script-console-run* ||
            "$script_console_output" != *child-return* ||
            "$script_console_output" != *child-global* ||
            "$script_console_history" != *view* ]]; then
        stop_smoke_process
        echo "error: $smoke_label script-console-view smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if (( visible_flipbook_delta <= 0 )); then
        stop_smoke_process
        echo "error: $smoke_label script-console-view smoke did not open a flipbook" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label script-console-view smoke launched and is still running: pid $pid"
        echo "Window: $script_console_window"
        echo "Flipbooks opened: $visible_flipbook_delta"
        echo "Output: $script_console_output"
        echo "History line: $script_console_history"
        echo "Run script ready: $script_console_run_script_ready"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label script-console-view smoke passed: $script_console_window"
      echo "Flipbooks opened: $visible_flipbook_delta"
      echo "Output: $script_console_output"
      echo "History line: $script_console_history"
      echo "Run script ready: $script_console_run_script_ready"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "preview-render-output" ]]; then
      if ! preview_render_window="$(wait_for_app_smoke_status preview-render-output 180)"; then
        stop_smoke_process
        echo "error: $smoke_label preview-render-output smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      preview_render_probe="$(status_value previewRenderProbe || true)"
      preview_render_ready="$(status_value previewRenderFrameReady || true)"
      preview_render_failed="$(status_value previewRenderFailedFrames || true)"
      preview_render_width="$(status_value previewRenderRasterWidth || true)"
      preview_render_height="$(status_value previewRenderRasterHeight || true)"
      preview_render_red_pixels="$(status_value previewRenderRedPixels || true)"
      preview_render_opaque_pixels="$(status_value previewRenderOpaquePixels || true)"
      if [[ "$preview_render_probe" != "ok" ||
            "$preview_render_ready" != "true" ||
            "$preview_render_failed" != "0" ||
            "$preview_render_width" != "320" ||
            "$preview_render_height" != "240" ||
            ! "$preview_render_red_pixels" =~ ^[0-9]+$ ||
            "$preview_render_red_pixels" == "0" ||
            ! "$preview_render_opaque_pixels" =~ ^[0-9]+$ ||
            "$preview_render_opaque_pixels" == "0" ]]; then
        stop_smoke_process
        echo "error: $smoke_label preview-render-output smoke reported unexpected state" >&2
        sed -n '1,140p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label preview-render-output smoke launched and is still running: pid $pid"
        echo "Window: $preview_render_window"
        echo "Preview raster size: ${preview_render_width}x${preview_render_height}"
        echo "Red pixels: $preview_render_red_pixels"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label preview-render-output smoke passed: $preview_render_window"
      echo "Preview raster size: ${preview_render_width}x${preview_render_height}"
      echo "Red pixels: $preview_render_red_pixels"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "fx-preview-render-output" ]]; then
      if ! fx_preview_render_window="$(wait_for_app_smoke_status fx-preview-render-output 180)"; then
        stop_smoke_process
        echo "error: $smoke_label fx-preview-render-output smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      fx_preview_render_probe="$(status_value fxPreviewRenderProbe || true)"
      fx_preview_render_ready="$(status_value fxPreviewRenderFrameReady || true)"
      fx_preview_render_flipbook="$(status_value fxPreviewRenderFlipbook || true)"
      fx_preview_render_attached="$(status_value fxPreviewRenderFlipbookAttached || true)"
      fx_preview_render_completed="$(status_value fxPreviewRenderCompletedFrames || true)"
      fx_preview_render_width="$(status_value fxPreviewRenderRasterWidth || true)"
      fx_preview_render_height="$(status_value fxPreviewRenderRasterHeight || true)"
      fx_preview_render_red_pixels="$(status_value fxPreviewRenderRedPixels || true)"
      fx_preview_render_opaque_pixels="$(status_value fxPreviewRenderOpaquePixels || true)"
      if [[ "$fx_preview_render_probe" != "ok" ||
            "$fx_preview_render_ready" != "true" ||
            "$fx_preview_render_flipbook" != "true" ||
            "$fx_preview_render_attached" != "true" ||
            ! "$fx_preview_render_completed" =~ ^[0-9]+$ ||
            "$fx_preview_render_completed" == "0" ||
            "$fx_preview_render_width" != "320" ||
            "$fx_preview_render_height" != "240" ||
            ! "$fx_preview_render_red_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_render_red_pixels" == "0" ||
            ! "$fx_preview_render_opaque_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_render_opaque_pixels" == "0" ]]; then
        stop_smoke_process
        echo "error: $smoke_label fx-preview-render-output smoke reported unexpected state" >&2
        sed -n '1,160p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label fx-preview-render-output smoke launched and is still running: pid $pid"
        echo "Window: $fx_preview_render_window"
        echo "FX preview raster size: ${fx_preview_render_width}x${fx_preview_render_height}"
        echo "Red pixels: $fx_preview_render_red_pixels"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label fx-preview-render-output smoke passed: $fx_preview_render_window"
      echo "FX preview raster size: ${fx_preview_render_width}x${fx_preview_render_height}"
      echo "Red pixels: $fx_preview_render_red_pixels"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "fx-preview-subcamera-render-output" ]]; then
      if ! fx_preview_subcamera_window="$(wait_for_app_smoke_status fx-preview-subcamera-render-output 180)"; then
        stop_smoke_process
        echo "error: $smoke_label fx-preview-subcamera-render-output smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      fx_preview_subcamera_probe="$(status_value fxPreviewSubcameraProbe || true)"
      fx_preview_subcamera_ready="$(status_value fxPreviewSubcameraFrameReady || true)"
      fx_preview_subcamera_flipbook="$(status_value fxPreviewSubcameraFlipbook || true)"
      fx_preview_subcamera_attached="$(status_value fxPreviewSubcameraFlipbookAttached || true)"
      fx_preview_subcamera_active="$(status_value fxPreviewSubcameraActive || true)"
      fx_preview_subcamera_completed="$(status_value fxPreviewSubcameraCompletedFrames || true)"
      fx_preview_subcamera_width="$(status_value fxPreviewSubcameraRasterWidth || true)"
      fx_preview_subcamera_height="$(status_value fxPreviewSubcameraRasterHeight || true)"
      fx_preview_subcamera_red_pixels="$(status_value fxPreviewSubcameraRedPixels || true)"
      fx_preview_subcamera_opaque_pixels="$(status_value fxPreviewSubcameraOpaquePixels || true)"
      if [[ "$fx_preview_subcamera_probe" != "ok" ||
            "$fx_preview_subcamera_ready" != "true" ||
            "$fx_preview_subcamera_flipbook" != "true" ||
            "$fx_preview_subcamera_attached" != "true" ||
            "$fx_preview_subcamera_active" != "true" ||
            ! "$fx_preview_subcamera_completed" =~ ^[0-9]+$ ||
            "$fx_preview_subcamera_completed" == "0" ||
            "$fx_preview_subcamera_width" != "160" ||
            "$fx_preview_subcamera_height" != "120" ||
            ! "$fx_preview_subcamera_red_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_subcamera_red_pixels" == "0" ||
            ! "$fx_preview_subcamera_opaque_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_subcamera_opaque_pixels" == "0" ]]; then
        stop_smoke_process
        echo "error: $smoke_label fx-preview-subcamera-render-output smoke reported unexpected state" >&2
        sed -n '1,160p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label fx-preview-subcamera-render-output smoke launched and is still running: pid $pid"
        echo "Window: $fx_preview_subcamera_window"
        echo "FX preview sub-camera raster size: ${fx_preview_subcamera_width}x${fx_preview_subcamera_height}"
        echo "Red pixels: $fx_preview_subcamera_red_pixels"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label fx-preview-subcamera-render-output smoke passed: $fx_preview_subcamera_window"
      echo "FX preview sub-camera raster size: ${fx_preview_subcamera_width}x${fx_preview_subcamera_height}"
      echo "Red pixels: $fx_preview_subcamera_red_pixels"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "fx-preview-flipbook-controls" ]]; then
      if ! fx_preview_flipbook_window="$(wait_for_app_smoke_status fx-preview-flipbook-controls 180)"; then
        stop_smoke_process
        echo "error: $smoke_label fx-preview-flipbook-controls smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      fx_preview_flipbook_probe="$(status_value fxPreviewFlipbookControlsProbe || true)"
      fx_preview_flipbook_attached="$(status_value fxPreviewFlipbookControlsFlipbookAttached || true)"
      fx_preview_flipbook_frame0_ready="$(status_value fxPreviewFlipbookControlsFrame0Ready || true)"
      fx_preview_flipbook_frame1_ready="$(status_value fxPreviewFlipbookControlsFrame1Ready || true)"
      fx_preview_flipbook_frame0_width="$(status_value fxPreviewFlipbookControlsFrame0RasterWidth || true)"
      fx_preview_flipbook_frame0_height="$(status_value fxPreviewFlipbookControlsFrame0RasterHeight || true)"
      fx_preview_flipbook_frame1_width="$(status_value fxPreviewFlipbookControlsFrame1RasterWidth || true)"
      fx_preview_flipbook_frame1_height="$(status_value fxPreviewFlipbookControlsFrame1RasterHeight || true)"
      fx_preview_flipbook_frame0_red_pixels="$(status_value fxPreviewFlipbookControlsFrame0RedPixels || true)"
      fx_preview_flipbook_frame1_green_pixels="$(status_value fxPreviewFlipbookControlsFrame1GreenPixels || true)"
      fx_preview_flipbook_frame_switch="$(status_value fxPreviewFlipbookControlsFrameSwitch || true)"
      fx_preview_flipbook_clone="$(status_value fxPreviewFlipbookControlsClone || true)"
      fx_preview_flipbook_freeze="$(status_value fxPreviewFlipbookControlsFreeze || true)"
      fx_preview_flipbook_unfreeze="$(status_value fxPreviewFlipbookControlsUnfreeze || true)"
      fx_preview_flipbook_frozen_frame0_red_pixels="$(status_value fxPreviewFlipbookControlsFrozenFrame0RedPixels || true)"
      fx_preview_flipbook_frozen_frame1_green_pixels="$(status_value fxPreviewFlipbookControlsFrozenFrame1GreenPixels || true)"
      if [[ "$fx_preview_flipbook_probe" != "ok" ||
            "$fx_preview_flipbook_attached" != "true" ||
            "$fx_preview_flipbook_frame0_ready" != "true" ||
            "$fx_preview_flipbook_frame1_ready" != "true" ||
            "$fx_preview_flipbook_frame0_width" != "320" ||
            "$fx_preview_flipbook_frame0_height" != "240" ||
            "$fx_preview_flipbook_frame1_width" != "320" ||
            "$fx_preview_flipbook_frame1_height" != "240" ||
            ! "$fx_preview_flipbook_frame0_red_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_flipbook_frame0_red_pixels" == "0" ||
            ! "$fx_preview_flipbook_frame1_green_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_flipbook_frame1_green_pixels" == "0" ||
            "$fx_preview_flipbook_frame_switch" != "true" ||
            "$fx_preview_flipbook_clone" != "true" ||
            "$fx_preview_flipbook_freeze" != "true" ||
            "$fx_preview_flipbook_unfreeze" != "true" ||
            ! "$fx_preview_flipbook_frozen_frame0_red_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_flipbook_frozen_frame0_red_pixels" == "0" ||
            ! "$fx_preview_flipbook_frozen_frame1_green_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_flipbook_frozen_frame1_green_pixels" == "0" ]]; then
        stop_smoke_process
        echo "error: $smoke_label fx-preview-flipbook-controls smoke reported unexpected state" >&2
        sed -n '1,180p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label fx-preview-flipbook-controls smoke launched and is still running: pid $pid"
        echo "Window: $fx_preview_flipbook_window"
        echo "FX preview frame 0 size: ${fx_preview_flipbook_frame0_width}x${fx_preview_flipbook_frame0_height}"
        echo "FX preview frame 1 size: ${fx_preview_flipbook_frame1_width}x${fx_preview_flipbook_frame1_height}"
        echo "Frame 0 red pixels: $fx_preview_flipbook_frame0_red_pixels"
        echo "Frame 1 green pixels: $fx_preview_flipbook_frame1_green_pixels"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label fx-preview-flipbook-controls smoke passed: $fx_preview_flipbook_window"
      echo "FX preview frame 0 size: ${fx_preview_flipbook_frame0_width}x${fx_preview_flipbook_frame0_height}"
      echo "FX preview frame 1 size: ${fx_preview_flipbook_frame1_width}x${fx_preview_flipbook_frame1_height}"
      echo "Frame 0 red pixels: $fx_preview_flipbook_frame0_red_pixels"
      echo "Frame 1 green pixels: $fx_preview_flipbook_frame1_green_pixels"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "fx-preview-save-previewed-frames" ||
          "$smoke_action" == "fx-preview-subcamera-save-previewed-frames" ]]; then
      if ! fx_preview_save_window="$(wait_for_app_smoke_status "$smoke_action" 180)"; then
        stop_smoke_process
        echo "error: $smoke_label $smoke_action smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$smoke_action" == "fx-preview-subcamera-save-previewed-frames" ]]; then
        fx_preview_save_expected_width="160"
        fx_preview_save_expected_height="120"
        fx_preview_save_expected_subcamera="true"
      else
        fx_preview_save_expected_width="320"
        fx_preview_save_expected_height="240"
        fx_preview_save_expected_subcamera="false"
      fi
      fx_preview_save_probe="$(status_value fxPreviewSavePreviewedFramesProbe || true)"
      fx_preview_save_subcamera="$(status_value fxPreviewSavePreviewedFramesSubcamera || true)"
      fx_preview_save_subcamera_active="$(status_value fxPreviewSavePreviewedFramesSubcameraActive || true)"
      fx_preview_save_reported_width="$(status_value fxPreviewSavePreviewedFramesExpectedWidth || true)"
      fx_preview_save_reported_height="$(status_value fxPreviewSavePreviewedFramesExpectedHeight || true)"
      fx_preview_save_attached="$(status_value fxPreviewSavePreviewedFramesFlipbookAttached || true)"
      fx_preview_save_frame0_ready="$(status_value fxPreviewSavePreviewedFramesFrame0Ready || true)"
      fx_preview_save_frame1_ready="$(status_value fxPreviewSavePreviewedFramesFrame1Ready || true)"
      fx_preview_save_started="$(status_value fxPreviewSavePreviewedFramesStarted || true)"
      fx_preview_save_output_probe="$(status_value fxPreviewSavePreviewedFramesOutputProbe || true)"
      fx_preview_save_output_count="$(status_value fxPreviewSavePreviewedFramesOutputCount || true)"
      fx_preview_save_output0_width="$(status_value fxPreviewSavePreviewedFramesOutput0Width || true)"
      fx_preview_save_output0_height="$(status_value fxPreviewSavePreviewedFramesOutput0Height || true)"
      fx_preview_save_output1_width="$(status_value fxPreviewSavePreviewedFramesOutput1Width || true)"
      fx_preview_save_output1_height="$(status_value fxPreviewSavePreviewedFramesOutput1Height || true)"
      fx_preview_save_output0_red_pixels="$(status_value fxPreviewSavePreviewedFramesOutput0RedPixels || true)"
      fx_preview_save_output1_green_pixels="$(status_value fxPreviewSavePreviewedFramesOutput1GreenPixels || true)"
      fx_preview_save_output0_bytes="$(status_value fxPreviewSavePreviewedFramesOutput0Bytes || true)"
      fx_preview_save_output1_bytes="$(status_value fxPreviewSavePreviewedFramesOutput1Bytes || true)"
      fx_preview_save_output_dir="$(status_value fxPreviewSavePreviewedFramesOutputDir || true)"
      if [[ "$fx_preview_save_probe" != "ok" ||
            "$fx_preview_save_subcamera" != "$fx_preview_save_expected_subcamera" ||
            "$fx_preview_save_reported_width" != "$fx_preview_save_expected_width" ||
            "$fx_preview_save_reported_height" != "$fx_preview_save_expected_height" ||
            ( "$fx_preview_save_expected_subcamera" == "true" && "$fx_preview_save_subcamera_active" != "true" ) ||
            "$fx_preview_save_attached" != "true" ||
            "$fx_preview_save_frame0_ready" != "true" ||
            "$fx_preview_save_frame1_ready" != "true" ||
            "$fx_preview_save_started" != "true" ||
            "$fx_preview_save_output_probe" != "ok" ||
            "$fx_preview_save_output_count" != "2" ||
            "$fx_preview_save_output0_width" != "$fx_preview_save_expected_width" ||
            "$fx_preview_save_output0_height" != "$fx_preview_save_expected_height" ||
            "$fx_preview_save_output1_width" != "$fx_preview_save_expected_width" ||
            "$fx_preview_save_output1_height" != "$fx_preview_save_expected_height" ||
            ! "$fx_preview_save_output0_red_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_save_output0_red_pixels" == "0" ||
            ! "$fx_preview_save_output1_green_pixels" =~ ^[0-9]+$ ||
            "$fx_preview_save_output1_green_pixels" == "0" ||
            ! "$fx_preview_save_output0_bytes" =~ ^[0-9]+$ ||
            "$fx_preview_save_output0_bytes" == "0" ||
            ! "$fx_preview_save_output1_bytes" =~ ^[0-9]+$ ||
            "$fx_preview_save_output1_bytes" == "0" ]]; then
        stop_smoke_process
        echo "error: $smoke_label $smoke_action smoke reported unexpected state" >&2
        sed -n '1,180p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label $smoke_action smoke launched and is still running: pid $pid"
        echo "Window: $fx_preview_save_window"
        echo "Saved preview frame 0 size: ${fx_preview_save_output0_width}x${fx_preview_save_output0_height}"
        echo "Saved preview frame 1 size: ${fx_preview_save_output1_width}x${fx_preview_save_output1_height}"
        echo "Frame 0 red pixels: $fx_preview_save_output0_red_pixels"
        echo "Frame 1 green pixels: $fx_preview_save_output1_green_pixels"
        echo "Output dir: $fx_preview_save_output_dir"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label $smoke_action smoke passed: $fx_preview_save_window"
      echo "Saved preview frame 0 size: ${fx_preview_save_output0_width}x${fx_preview_save_output0_height}"
      echo "Saved preview frame 1 size: ${fx_preview_save_output1_width}x${fx_preview_save_output1_height}"
      echo "Frame 0 red pixels: $fx_preview_save_output0_red_pixels"
      echo "Frame 1 green pixels: $fx_preview_save_output1_green_pixels"
      echo "Output dir: $fx_preview_save_output_dir"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "final-render-output" ||
          "$smoke_action" == "final-render-background-output" ||
          "$smoke_action" == "final-render-sequence-output" ||
          "$smoke_action" == "final-render-composite-output" ||
          "$smoke_action" == "final-render-vector-output" ||
          "$smoke_action" == "final-render-toonz-raster-output" ||
          "$smoke_action" == "final-render-fx-output" ]]; then
      if ! final_render_window="$(wait_for_app_smoke_status "$smoke_action" 180)"; then
        stop_smoke_process
        echo "error: $smoke_label $smoke_action smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      final_render_probe="$(status_value finalRenderProbe || true)"
      final_render_output_probe="$(status_value finalRenderOutputProbe || true)"
      final_render_completed="$(status_value finalRenderCompletedFrames || true)"
      final_render_failed="$(status_value finalRenderFailedFrames || true)"
      final_render_width="$(status_value finalRenderOutputWidth || true)"
      final_render_height="$(status_value finalRenderOutputHeight || true)"
      final_render_red_pixels="$(status_value finalRenderOutputRedPixels || true)"
      final_render_green_pixels="$(status_value finalRenderOutputGreenPixels || true)"
      final_render_blue_pixels="$(status_value finalRenderOutputBluePixels || true)"
      final_render_frame_count="$(status_value finalRenderOutputFrameCount || true)"
      final_render_output_path="$(status_value finalRenderOutputPath || true)"
      final_render_second_width="$(status_value finalRenderSecondOutputWidth || true)"
      final_render_second_height="$(status_value finalRenderSecondOutputHeight || true)"
      final_render_second_green_pixels="$(status_value finalRenderSecondOutputGreenPixels || true)"
      final_render_second_output_path="$(status_value finalRenderSecondOutputPath || true)"
      final_render_fx_applied="$(status_value finalRenderFxApplied || true)"
      final_render_fx_root="$(status_value finalRenderFxRoot || true)"
      final_render_background_ok=1
      if [[ "$smoke_action" == "final-render-background-output" &&
            ( ! "$final_render_blue_pixels" =~ ^[0-9]+$ ||
              "$final_render_blue_pixels" == "0" ) ]]; then
        final_render_background_ok=0
      fi
      final_render_composite_ok=1
      if [[ "$smoke_action" == "final-render-composite-output" &&
            ( ! "$final_render_green_pixels" =~ ^[0-9]+$ ||
              "$final_render_green_pixels" == "0" ) ]]; then
        final_render_composite_ok=0
      fi
      final_render_expected_frames=1
      final_render_sequence_ok=1
      if [[ "$smoke_action" == "final-render-sequence-output" ]]; then
        final_render_expected_frames=2
        if [[ "$final_render_frame_count" != "2" ||
              "$final_render_second_width" != "320" ||
              "$final_render_second_height" != "240" ||
              ! "$final_render_second_green_pixels" =~ ^[0-9]+$ ||
              "$final_render_second_green_pixels" == "0" ||
              -z "$final_render_second_output_path" ||
              ! -s "$final_render_second_output_path" ]]; then
          final_render_sequence_ok=0
        fi
      fi
      final_render_fx_ok=1
      if [[ "$smoke_action" == "final-render-fx-output" &&
            ( "$final_render_fx_applied" != "true" ||
              "$final_render_fx_root" != STD_blurFx:* ) ]]; then
        final_render_fx_ok=0
      fi
      if [[ "$final_render_probe" != "ok" ||
            "$final_render_output_probe" != "ok" ||
            "$final_render_completed" != "$final_render_expected_frames" ||
            "$final_render_failed" != "0" ||
            "$final_render_width" != "320" ||
            "$final_render_height" != "240" ||
            ! "$final_render_red_pixels" =~ ^[0-9]+$ ||
            "$final_render_red_pixels" == "0" ||
            "$final_render_background_ok" != "1" ||
            "$final_render_composite_ok" != "1" ||
            "$final_render_sequence_ok" != "1" ||
            "$final_render_fx_ok" != "1" ||
            -z "$final_render_output_path" ||
            ! -s "$final_render_output_path" ]]; then
        stop_smoke_process
        echo "error: $smoke_label $smoke_action smoke reported unexpected state" >&2
        sed -n '1,140p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label $smoke_action smoke launched and is still running: pid $pid"
        echo "Window: $final_render_window"
        echo "Output: $final_render_output_path"
        echo "Output size: ${final_render_width}x${final_render_height}"
        echo "Red pixels: $final_render_red_pixels"
        if [[ "$smoke_action" == "final-render-background-output" ]]; then
          echo "Blue pixels: $final_render_blue_pixels"
        fi
        if [[ "$smoke_action" == "final-render-composite-output" ]]; then
          echo "Green pixels: $final_render_green_pixels"
        fi
        if [[ "$smoke_action" == "final-render-sequence-output" ]]; then
          echo "Output frames: $final_render_frame_count"
          echo "Second output: $final_render_second_output_path"
          echo "Second output size: ${final_render_second_width}x${final_render_second_height}"
          echo "Second green pixels: $final_render_second_green_pixels"
        fi
        if [[ "$smoke_action" == "final-render-fx-output" ]]; then
          echo "FX root: $final_render_fx_root"
        fi
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label $smoke_action smoke passed: $final_render_window"
      echo "Output: $final_render_output_path"
      echo "Output size: ${final_render_width}x${final_render_height}"
      echo "Red pixels: $final_render_red_pixels"
      if [[ "$smoke_action" == "final-render-background-output" ]]; then
        echo "Blue pixels: $final_render_blue_pixels"
      fi
      if [[ "$smoke_action" == "final-render-composite-output" ]]; then
        echo "Green pixels: $final_render_green_pixels"
      fi
      if [[ "$smoke_action" == "final-render-sequence-output" ]]; then
        echo "Output frames: $final_render_frame_count"
        echo "Second output: $final_render_second_output_path"
        echo "Second output size: ${final_render_second_width}x${final_render_second_height}"
        echo "Second green pixels: $final_render_second_green_pixels"
      fi
      if [[ "$smoke_action" == "final-render-fx-output" ]]; then
        echo "FX root: $final_render_fx_root"
      fi
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "audio-playback-wav" ]]; then
      if ! playback_window="$(wait_for_app_smoke_status audio-playback-wav)"; then
        stop_smoke_process
        echo "error: $smoke_label audio-playback-wav smoke failed" >&2
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
        echo "error: $smoke_label audio-playback-wav smoke reported unexpected state" >&2
        sed -n '1,150p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label audio-playback-wav smoke launched and is still running: pid $pid"
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
      echo "$smoke_label audio-playback-wav smoke passed: $playback_window"
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
        echo "error: $smoke_label xsheet-scrub smoke failed" >&2
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
        echo "error: $smoke_label xsheet-scrub smoke reported unexpected state" >&2
        sed -n '1,80p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label xsheet-scrub smoke launched and is still running: pid $pid"
        echo "Window: $xsheet_window"
        echo "Scene: $created_scene_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label xsheet-scrub smoke passed: $xsheet_window"
      echo "Scene: $created_scene_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$smoke_action" == "viewer-render" || "$smoke_action" == "viewer-vector-render" || "$smoke_action" == "viewer-zoom-pan" || "$smoke_action" == "viewer-onion-skin" || "$smoke_action" == "viewer-onion-skin-rowarea" || "$smoke_action" == "viewer-onion-skin-rowarea-drag" || "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" || "$smoke_action" == "viewer-onion-skin-shift-trace" || "$smoke_action" == "viewer-onion-skin-context-menu" || "$smoke_action" == "viewer-onion-skin-custom-colors" || "$smoke_action" == "viewer-onion-skin-orientations" || "$smoke_action" == "viewer-camera-overlay" || "$smoke_action" == "viewer-safe-area-field-guide" || "$smoke_action" == "viewer-safe-area-presets" || "$smoke_action" == "viewer-safe-area-custom-file" || "$smoke_action" == "viewer-field-guide-settings" || "$smoke_action" == "viewer-ruler-guide" || "$smoke_action" == "viewer-ruler-guide-events" || "$smoke_action" == "viewer-ruler-guide-variants" || "$smoke_action" == "viewer-ruler-guide-lines" || "$smoke_action" == "viewer-ruler-guide-styles" || "$smoke_action" == "viewer-ruler-ticks" || "$smoke_action" == "viewer-animate-tool-overlay" || "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-animate-tool-cursors" || "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" || "$smoke_action" == "viewer-vector-brush" || "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
      viewer_status_attempts=100
      if [[ "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        viewer_status_attempts=400
      fi
      if ! viewer_window="$(wait_for_app_smoke_status "$smoke_action" "$viewer_status_attempts")"; then
        stop_smoke_process
        echo "error: $smoke_label ${smoke_action} smoke failed" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      viewer_probe="$(status_value viewerRenderProbe || true)"
      viewer_high_dpi_probe="$(status_value viewerHighDpiProbe || true)"
      viewer_content="$(status_value viewerRenderContent || true)"
      viewer_logical_width="$(status_value viewerLogicalWidth || true)"
      viewer_logical_height="$(status_value viewerLogicalHeight || true)"
      viewer_dev_pix_ratio="$(status_value viewerDevPixRatio || true)"
      viewer_device_pixel_ratio="$(status_value viewerDevicePixelRatio || true)"
      viewer_screen_device_pixel_ratio="$(status_value viewerScreenDevicePixelRatio || true)"
      framebuffer_width="$(status_value framebufferWidth || true)"
      framebuffer_height="$(status_value framebufferHeight || true)"
      changed_pixels="$(status_value changedPixels || true)"
      red_pixels="$(status_value redPixels || true)"
      changed_neutral_pixels="$(status_value changedNeutralPixels || true)"
      onion_pixels="$(status_value onionPixels || true)"
      baseline_onion_pixels="$(status_value baselineOnionPixels || true)"
      before_capture_saved="$(status_value beforeCaptureSaved || true)"
      after_capture_saved="$(status_value afterCaptureSaved || true)"
      before_capture_path="$(status_value beforeCapturePath || true)"
      after_capture_path="$(status_value afterCapturePath || true)"
      stale_frame_probe="ok"
      stale_frame_changed_pixels="0"
      stale_frame_green_pixels="0"
      stale_frame_changed_green_pixels="0"
      stale_frame_capture_saved="true"
      stale_frame_capture_path="$after_capture_path"
      if [[ "$smoke_action" == "viewer-render" ]]; then
        stale_frame_probe="$(status_value viewerStaleFrameProbe || true)"
        stale_frame_changed_pixels="$(status_value viewerStaleFrameChangedPixels || true)"
        stale_frame_green_pixels="$(status_value viewerStaleFrameGreenPixels || true)"
        stale_frame_changed_green_pixels="$(status_value viewerStaleFrameChangedGreenPixels || true)"
        stale_frame_capture_saved="$(status_value viewerStaleFrameCaptureSaved || true)"
        stale_frame_capture_path="$(status_value viewerStaleFrameCapturePath || true)"
      fi
      viewer_transform_probe="ok"
      if [[ "$smoke_action" == "viewer-zoom-pan" ]]; then
        viewer_transform_probe="$(status_value viewerTransformProbe || true)"
      fi
      onion_skin_probe="ok"
      onion_skin_ui_probe="ok"
      onion_skin_drag_probe="ok"
      onion_skin_menu_probe="ok"
      onion_skin_custom_color_probe="ok"
      onion_skin_orientation_probe="ok"
      xsheet_row_area_probe="ok"
      xsheet_row_area_high_dpi_probe="ok"
      if [[ "$smoke_action" == "viewer-onion-skin" || "$smoke_action" == "viewer-onion-skin-rowarea" || "$smoke_action" == "viewer-onion-skin-rowarea-drag" || "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" || "$smoke_action" == "viewer-onion-skin-shift-trace" || "$smoke_action" == "viewer-onion-skin-context-menu" || "$smoke_action" == "viewer-onion-skin-custom-colors" || "$smoke_action" == "viewer-onion-skin-orientations" ]]; then
        onion_skin_probe="$(status_value onionSkinProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-rowarea" ]]; then
        onion_skin_ui_probe="$(status_value onionSkinUiProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-rowarea-drag" ]]; then
        onion_skin_drag_probe="$(status_value onionSkinDragProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      onion_skin_fixed_drag_probe="ok"
      if [[ "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" ]]; then
        onion_skin_fixed_drag_probe="$(status_value onionSkinFixedDragProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      onion_skin_shift_trace_probe="ok"
      if [[ "$smoke_action" == "viewer-onion-skin-shift-trace" ]]; then
        onion_skin_shift_trace_probe="$(status_value onionSkinShiftTraceProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-context-menu" ]]; then
        onion_skin_menu_probe="$(status_value onionSkinMenuProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-custom-colors" ]]; then
        onion_skin_custom_color_probe="$(status_value onionSkinCustomColorProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-orientations" ]]; then
        onion_skin_orientation_probe="$(status_value onionSkinOrientationProbe || true)"
        onion_skin_ui_probe="$(status_value onionSkinUiProbe || true)"
        xsheet_row_area_probe="$(status_value xsheetRowAreaProbe || true)"
        xsheet_row_area_high_dpi_probe="$(status_value xsheetRowAreaHighDpiProbe || true)"
      fi
      safe_area_probe="ok"
      field_guide_probe="ok"
      if [[ "$smoke_action" == "viewer-safe-area-field-guide" ]]; then
        safe_area_probe="$(status_value safeAreaProbe || true)"
        field_guide_probe="$(status_value fieldGuideProbe || true)"
      fi
      safe_area_preset_probe="ok"
      if [[ "$smoke_action" == "viewer-safe-area-presets" ]]; then
        safe_area_preset_probe="$(status_value safeAreaPresetProbe || true)"
      fi
      safe_area_custom_file_probe="ok"
      safe_area_custom_file_restored="true"
      if [[ "$smoke_action" == "viewer-safe-area-custom-file" ]]; then
        safe_area_custom_file_probe="$(status_value safeAreaCustomFileProbe || true)"
        safe_area_custom_file_restored="$(status_value safeAreaCustomFileRestored || true)"
      fi
      field_guide_settings_probe="ok"
      if [[ "$smoke_action" == "viewer-field-guide-settings" ]]; then
        field_guide_settings_probe="$(status_value fieldGuideSettingsProbe || true)"
      fi
      ruler_guide_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-guide" ]]; then
        ruler_guide_probe="$(status_value rulerGuideProbe || true)"
      fi
      ruler_guide_event_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-guide-events" ]]; then
        ruler_guide_event_probe="$(status_value rulerGuideEventProbe || true)"
      fi
      ruler_guide_variant_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-guide-variants" ]]; then
        ruler_guide_variant_probe="$(status_value rulerGuideVariantProbe || true)"
      fi
      ruler_guide_line_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-guide-lines" ]]; then
        ruler_guide_line_probe="$(status_value rulerGuideLineProbe || true)"
      fi
      ruler_widget_high_dpi_probe="ok"
      ruler_style_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-guide-styles" ]]; then
        ruler_style_probe="$(status_value rulerStyleProbe || true)"
        ruler_widget_high_dpi_probe="$(status_value rulerWidgetHighDpiProbe || true)"
      fi
      ruler_tick_probe="ok"
      ruler_transform_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-ticks" ]]; then
        ruler_tick_probe="$(status_value rulerTickProbe || true)"
        ruler_widget_high_dpi_probe="$(status_value rulerWidgetHighDpiProbe || true)"
        ruler_transform_probe="$(status_value rulerTransformProbe || true)"
      fi
      tool_overlay_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-overlay" ]]; then
        tool_overlay_probe="$(status_value toolOverlayProbe || true)"
      fi
      tool_transform_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" ]]; then
        tool_transform_probe="$(status_value toolTransformProbe || true)"
      fi
      handle_hover_probe="ok"
      handle_hit_test_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-handles" ]]; then
        handle_hover_probe="$(status_value handleHoverProbe || true)"
        handle_hit_test_probe="$(status_value handleHitTestProbe || true)"
      fi
      handle_variant_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-handle-variants" ]]; then
        handle_variant_probe="$(status_value handleVariantProbe || true)"
      fi
      axis_drag_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-axis-drags" ]]; then
        axis_drag_probe="$(status_value axisDragProbe || true)"
      fi
      animate_cursor_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-cursors" ]]; then
        animate_cursor_probe="$(status_value animateCursorProbe || true)"
      fi
      selection_tool_probe="ok"
      selection_rect_probe="ok"
      selection_cursor_probe="ok"
      selection_handle_probe="ok"
      selection_variant_cursor_probe="ok"
      selection_variant_handle_probe="ok"
      selection_advanced_cursor_probe="ok"
      selection_advanced_handle_probe="ok"
      selection_raster_cursor_probe="ok"
      selection_raster_handle_probe="ok"
      selection_freehand_probe="ok"
      selection_polyline_probe="ok"
      if [[ "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" ]]; then
        selection_tool_probe="$(status_value selectionToolProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-raster-handles" ]]; then
        selection_rect_probe="$(status_value selectionRectProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-handles" ]]; then
        selection_cursor_probe="$(status_value selectionCursorProbe || true)"
        selection_handle_probe="$(status_value selectionHandleProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-handle-variants" ]]; then
        selection_variant_cursor_probe="$(status_value selectionVariantCursorProbe || true)"
        selection_variant_handle_probe="$(status_value selectionVariantHandleProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" ]]; then
        selection_advanced_cursor_probe="$(status_value selectionAdvancedCursorProbe || true)"
        selection_advanced_handle_probe="$(status_value selectionAdvancedHandleProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-raster-handles" ]]; then
        selection_raster_cursor_probe="$(status_value selectionRasterCursorProbe || true)"
        selection_raster_handle_probe="$(status_value selectionRasterHandleProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" ]]; then
        selection_freehand_probe="$(status_value selectionFreehandProbe || true)"
        selection_polyline_probe="$(status_value selectionPolylineProbe || true)"
      fi
      undo_redo_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-undo-redo" ]]; then
        undo_redo_probe="$(status_value undoRedoProbe || true)"
      fi
      modifier_probe="ok"
      cursor_precise_probe="ok"
      if [[ "$smoke_action" == "viewer-animate-tool-modifiers" ]]; then
        modifier_probe="$(status_value modifierProbe || true)"
        cursor_precise_probe="$(status_value cursorPreciseProbe || true)"
      fi
      changed_pixels_probe="error"
      if [[ "$changed_pixels" =~ ^[0-9]+$ ]]; then
        if [[ "$smoke_action" == "viewer-onion-skin" || "$smoke_action" == "viewer-onion-skin-rowarea" || "$smoke_action" == "viewer-onion-skin-rowarea-drag" || "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" || "$smoke_action" == "viewer-onion-skin-shift-trace" || "$smoke_action" == "viewer-onion-skin-context-menu" || "$smoke_action" == "viewer-onion-skin-custom-colors" || "$smoke_action" == "viewer-onion-skin-orientations" || "$changed_pixels" != "0" ]]; then
          changed_pixels_probe="ok"
        fi
      fi
      red_pixels_probe="ok"
      if [[ "$smoke_action" != "viewer-field-guide-settings" && "$smoke_action" != "viewer-ruler-guide" && "$smoke_action" != "viewer-ruler-guide-events" && "$smoke_action" != "viewer-ruler-guide-variants" && "$smoke_action" != "viewer-ruler-guide-lines" && "$smoke_action" != "viewer-ruler-guide-styles" ]]; then
        red_pixels_probe="error"
        if [[ "$red_pixels" =~ ^[0-9]+$ && "$red_pixels" != "0" ]]; then
          red_pixels_probe="ok"
        fi
      fi
      neutral_pixels_probe="ok"
      if [[ "$smoke_action" == "viewer-ruler-guide" || "$smoke_action" == "viewer-ruler-guide-events" || "$smoke_action" == "viewer-ruler-guide-variants" || "$smoke_action" == "viewer-ruler-guide-lines" || "$smoke_action" == "viewer-ruler-guide-styles" ]]; then
        neutral_pixels_probe="error"
        if [[ "$changed_neutral_pixels" =~ ^[0-9]+$ &&
              "$changed_neutral_pixels" != "0" ]]; then
          neutral_pixels_probe="ok"
        fi
      fi
      onion_pixels_probe="ok"
      if [[ "$smoke_action" == "viewer-onion-skin" || "$smoke_action" == "viewer-onion-skin-rowarea" || "$smoke_action" == "viewer-onion-skin-rowarea-drag" || "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" || "$smoke_action" == "viewer-onion-skin-shift-trace" || "$smoke_action" == "viewer-onion-skin-context-menu" || "$smoke_action" == "viewer-onion-skin-orientations" ]]; then
        onion_pixels_probe="error"
        if [[ "$onion_pixels" =~ ^[0-9]+$ &&
              "$baseline_onion_pixels" =~ ^[0-9]+$ &&
              "$onion_pixels" != "0" ]] &&
           ((onion_pixels > baseline_onion_pixels)); then
          onion_pixels_probe="ok"
        fi
      fi
      tool_input_probe="ok"
      raster_input_probe="ok"
      if [[ "$smoke_action" == "viewer-vector-brush" || "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        tool_input_probe="$(status_value toolInputProbe || true)"
      fi
      if [[ "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        raster_input_probe="$(status_value rasterInputProbe || true)"
      fi
      mouse_event_probe="ok"
      if [[ "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-animate-tool-cursors" || "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" ]]; then
        mouse_event_probe="$(status_value mouseEventProbe || true)"
      fi
      tablet_event_probe="ok"
      if [[ "$smoke_action" == "viewer-raster-brush-tablet-events" ]]; then
        tablet_event_probe="$(status_value tabletEventProbe || true)"
      fi
      system_mouse_event_probe="ok"
      if [[ "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        system_mouse_event_probe="$(status_value systemMouseEventProbe || true)"
      fi
      if [[ "$viewer_probe" != "ok" ||
            "$viewer_high_dpi_probe" != "ok" ||
            "$tool_input_probe" != "ok" ||
            "$raster_input_probe" != "ok" ||
            "$viewer_transform_probe" != "ok" ||
            "$onion_skin_probe" != "ok" ||
            "$onion_skin_ui_probe" != "ok" ||
            "$onion_skin_drag_probe" != "ok" ||
            "$onion_skin_fixed_drag_probe" != "ok" ||
            "$onion_skin_shift_trace_probe" != "ok" ||
            "$onion_skin_menu_probe" != "ok" ||
            "$onion_skin_custom_color_probe" != "ok" ||
            "$onion_skin_orientation_probe" != "ok" ||
            "$xsheet_row_area_probe" != "ok" ||
            "$xsheet_row_area_high_dpi_probe" != "ok" ||
            "$safe_area_probe" != "ok" ||
            "$safe_area_preset_probe" != "ok" ||
            "$safe_area_custom_file_probe" != "ok" ||
            "$safe_area_custom_file_restored" != "true" ||
            "$field_guide_probe" != "ok" ||
            "$field_guide_settings_probe" != "ok" ||
            "$ruler_guide_probe" != "ok" ||
            "$ruler_guide_event_probe" != "ok" ||
            "$ruler_guide_variant_probe" != "ok" ||
            "$ruler_guide_line_probe" != "ok" ||
            "$ruler_style_probe" != "ok" ||
            "$ruler_tick_probe" != "ok" ||
            "$ruler_widget_high_dpi_probe" != "ok" ||
            "$ruler_transform_probe" != "ok" ||
            "$tool_overlay_probe" != "ok" ||
            "$tool_transform_probe" != "ok" ||
            "$handle_hover_probe" != "ok" ||
            "$handle_hit_test_probe" != "ok" ||
            "$handle_variant_probe" != "ok" ||
            "$axis_drag_probe" != "ok" ||
            "$animate_cursor_probe" != "ok" ||
            "$selection_tool_probe" != "ok" ||
            "$selection_rect_probe" != "ok" ||
            "$selection_cursor_probe" != "ok" ||
            "$selection_handle_probe" != "ok" ||
            "$selection_variant_cursor_probe" != "ok" ||
            "$selection_variant_handle_probe" != "ok" ||
            "$selection_advanced_cursor_probe" != "ok" ||
            "$selection_advanced_handle_probe" != "ok" ||
            "$selection_raster_cursor_probe" != "ok" ||
            "$selection_raster_handle_probe" != "ok" ||
            "$selection_freehand_probe" != "ok" ||
            "$selection_polyline_probe" != "ok" ||
            "$undo_redo_probe" != "ok" ||
            "$modifier_probe" != "ok" ||
            "$cursor_precise_probe" != "ok" ||
            "$onion_pixels_probe" != "ok" ||
            "$mouse_event_probe" != "ok" ||
            "$tablet_event_probe" != "ok" ||
            "$system_mouse_event_probe" != "ok" ||
            -z "$viewer_content" ||
            ! "$framebuffer_width" =~ ^[0-9]+$ ||
            ! "$framebuffer_height" =~ ^[0-9]+$ ||
            "$changed_pixels_probe" != "ok" ||
            "$red_pixels_probe" != "ok" ||
            "$neutral_pixels_probe" != "ok" ||
            "$stale_frame_probe" != "ok" ||
            ! "$stale_frame_changed_pixels" =~ ^[0-9]+$ ||
            ! "$stale_frame_green_pixels" =~ ^[0-9]+$ ||
            ! "$stale_frame_changed_green_pixels" =~ ^[0-9]+$ ||
            "$stale_frame_capture_saved" != "true" ||
            ! -f "$stale_frame_capture_path" ||
            "$framebuffer_width" == "0" ||
            "$framebuffer_height" == "0" ||
            "$before_capture_saved" != "true" ||
            "$after_capture_saved" != "true" ||
            ! -f "$before_capture_path" ||
            ! -f "$after_capture_path" ]]; then
        stop_smoke_process
        echo "error: $smoke_label ${smoke_action} smoke reported unexpected state" >&2
        sed -n '1,120p' "$status_path" >&2
        sed -n '1,180p' "$log_path" >&2
        exit 1
      fi

      if [[ "$hold_app" == "1" ]]; then
        echo "$smoke_label ${smoke_action} smoke launched and is still running: pid $pid"
        echo "Window: $viewer_window"
        echo "Content: $viewer_content"
        echo "Viewer logical size: ${viewer_logical_width}x${viewer_logical_height}"
        echo "Viewer DPR: ${viewer_dev_pix_ratio} (${viewer_device_pixel_ratio}; screen ${viewer_screen_device_pixel_ratio})"
        if [[ "$smoke_action" == "viewer-zoom-pan" ]]; then
          echo "Transform: zoom $(status_value viewerZoomFactor || true), pan $(status_value viewerPanLogicalX || true),$(status_value viewerPanLogicalY || true) logical pixels"
          echo "Viewer scale: $(status_value viewerScaleBefore || true) -> $(status_value viewerScaleAfter || true)"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin" ]]; then
          echo "Onion skin: row $(status_value onionSkinCurrentRow || true), back offset $(status_value onionSkinBackOffset || true), mobile count $(status_value onionSkinMosCount || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-rowarea" ]]; then
          echo "Onion row area: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true)"
          echo "Onion toggles: markers $(status_value onionSkinEnabledAfterMarkers || true), disabled $(status_value onionSkinEnabledAfterDisable || true), enabled $(status_value onionSkinEnabledAfterEnable || true)"
          echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-rowarea-drag" ]]; then
          echo "Onion row area drag: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true), range $(status_value onionSkinMosRange || true)"
          echo "Row area drag: delivered $(status_value rowAreaDragEventDelivered || true), points $(status_value rowAreaDragPoints || true)"
          echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" ]]; then
          echo "Fixed onion marker drag: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true)"
          echo "Fixed onion marker counts: add $(status_value onionSkinFosCountAfterAddBack || true), remove $(status_value onionSkinFosCountAfterRemoveBack || true), final $(status_value onionSkinFosCount || true)"
          echo "Fixed onion marker events: add $(status_value rowAreaAddFixedDragDelivered || true), remove $(status_value rowAreaRemoveFixedDragDelivered || true), front $(status_value rowAreaAddFrontFixedDragDelivered || true)"
          echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-shift-trace" ]]; then
          echo "Shift trace offsets: initial $(status_value shiftTraceInitialOffsets || true), moved $(status_value shiftTraceAfterMoveFrontOffsets || true), reset $(status_value shiftTraceAfterResetOffsets || true), hidden $(status_value shiftTraceAfterHideFrontOffsets || true), final $(status_value shiftTraceFinalOffsets || true)"
          echo "Shift trace events: move back $(status_value shiftTraceMoveBackEventDelivered || true), move front $(status_value shiftTraceMoveFrontEventDelivered || true), reset $(status_value shiftTraceResetEventDelivered || true), hide back $(status_value shiftTraceHideBackEventDelivered || true), hide front $(status_value shiftTraceHideFrontEventDelivered || true)"
          echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-context-menu" ]]; then
          echo "Onion context menu: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true), range $(status_value onionSkinMosRange || true)"
          echo "Onion commands: activate $(status_value onionSkinActivateCommand || true), deactivate $(status_value onionSkinDeactivateCommand || true), extend $(status_value onionSkinExtendCommand || true), limit $(status_value onionSkinLimitCommand || true), clear fixed $(status_value onionSkinClearFixedCommand || true), clear relative $(status_value onionSkinClearRelativeCommand || true), clear all $(status_value onionSkinClearAllCommand || true)"
          echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-custom-colors" ]]; then
          echo "Onion custom colors: back $(status_value onionSkinBackColor || true), front $(status_value onionSkinFrontColor || true)"
          echo "Onion color pixels: viewer back $(status_value viewerBackColorPixels || true), viewer front $(status_value viewerFrontColorPixels || true), row back $(status_value xsheetRowAreaBackColorPixels || true), row front $(status_value xsheetRowAreaFrontColorPixels || true)"
          echo "Onion rows: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), rows $(status_value onionSkinRows || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-onion-skin-orientations" ]]; then
          echo "Onion orientations: $(status_value orientationBefore || true) -> $(status_value orientationAfter || true)"
          echo "Vertical row area pixels: changed $(status_value verticalRowAreaChangedPixels || true), non-background $(status_value verticalRowAreaNonBackgroundPixels || true)"
          echo "Horizontal row area pixels: changed $(status_value horizontalRowAreaChangedPixels || true), non-background $(status_value horizontalRowAreaNonBackgroundPixels || true)"
          echo "Onion rows: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true)"
          echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
        fi
        if [[ "$smoke_action" == "viewer-camera-overlay" ]]; then
          echo "Camera overlay: disabled $(status_value cameraOverlayDisabled || true), enabled $(status_value cameraOverlayEnabled || true)"
        fi
        if [[ "$smoke_action" == "viewer-safe-area-field-guide" ]]; then
          echo "Safe area: disabled $(status_value safeAreaDisabled || true), enabled $(status_value safeAreaEnabled || true)"
          echo "Field guide: disabled $(status_value fieldGuideDisabled || true), enabled $(status_value fieldGuideEnabled || true)"
          echo "Gray pixels: $(status_value grayPixels || true), changed gray pixels: $(status_value changedGrayPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-safe-area-presets" ]]; then
          echo "Safe area preset: $(status_value safeAreaNameDefault || true) -> $(status_value safeAreaNameCustom || true), stored $(status_value storedSafeAreaName || true)"
          echo "Default safe area pixels: red $(status_value defaultSafeAreaRedPixels || true), changed red $(status_value defaultSafeAreaChangedRedPixels || true)"
          echo "Custom safe area pixels: red $(status_value customSafeAreaRedPixels || true), green $(status_value customSafeAreaGreenPixels || true), blue $(status_value customSafeAreaBluePixels || true)"
          echo "Safe area preset delta: changed $(status_value safeAreaPresetChangedPixels || true), green $(status_value safeAreaPresetChangedGreenPixels || true), blue $(status_value safeAreaPresetChangedBluePixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-safe-area-custom-file" ]]; then
          echo "Custom safe area file: written $(status_value customSafeAreaFileWritten || true), restored $(status_value safeAreaCustomFileRestored || true)"
          echo "Custom safe area preset: $(status_value customSafeAreaName || true), stored $(status_value storedSafeAreaName || true)"
          echo "Custom safe area pixels: red $(status_value customSafeAreaRedPixels || true), green $(status_value customSafeAreaGreenPixels || true), blue $(status_value customSafeAreaBluePixels || true)"
          echo "Custom safe area changed pixels: red $(status_value customSafeAreaChangedRedPixels || true), green $(status_value customSafeAreaChangedGreenPixels || true), blue $(status_value customSafeAreaChangedBluePixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-field-guide-settings" ]]; then
          echo "Field guide overlay: disabled $(status_value fieldGuideDisabled || true), enabled $(status_value fieldGuideEnabled || true)"
          echo "Field guide settings: size $(status_value initialFieldGuideSize || true) -> $(status_value firstFieldGuideSize || true) -> $(status_value secondFieldGuideSize || true), aspect $(status_value initialFieldGuideAspectRatio || true) -> $(status_value firstFieldGuideAspectRatio || true) -> $(status_value secondFieldGuideAspectRatio || true)"
          echo "Field guide pixels: first gray $(status_value firstFieldGuideGrayPixels || true), changed $(status_value firstFieldGuideChangedGrayPixels || true); second gray $(status_value secondFieldGuideGrayPixels || true), changed $(status_value secondFieldGuideChangedGrayPixels || true)"
          echo "Field guide settings delta: changed $(status_value fieldGuideSettingsChangedPixels || true), gray $(status_value fieldGuideSettingsChangedGrayPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-ruler-guide" ]]; then
          echo "Guide overlay: disabled $(status_value viewGuideDisabled || true), enabled $(status_value viewGuideEnabled || true)"
          echo "Ruler overlay: disabled $(status_value viewRulerDisabled || true), enabled $(status_value viewRulerEnabled || true)"
          echo "Guides: H $(status_value hGuideCount || true), V $(status_value vGuideCount || true)"
          echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
          echo "Gray pixels: $(status_value grayPixels || true), changed gray pixels: $(status_value changedGrayPixels || true), changed neutral pixels: $(status_value changedNeutralPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-ruler-guide-events" ]]; then
          echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
          echo "Horizontal guides: $(status_value horizontalGuideCountBefore || true) -> $(status_value horizontalGuideCountAfterMove || true) -> $(status_value horizontalGuideCountAfterDelete || true)"
          echo "Vertical guides: $(status_value verticalGuideCountBefore || true) -> $(status_value verticalGuideCountAfterMove || true) -> $(status_value verticalGuideCountAfterDelete || true)"
          echo "Guide values: H $(status_value horizontalGuideStart || true) -> $(status_value horizontalGuideAfterMove || true) -> deleted, V $(status_value verticalGuideStart || true) -> $(status_value verticalGuideAfterMove || true)"
          echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
          echo "Gray pixels: $(status_value grayPixels || true), changed gray pixels: $(status_value changedGrayPixels || true), changed neutral pixels: $(status_value changedNeutralPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-ruler-guide-variants" ]]; then
          echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
          echo "Horizontal hide variant: $(status_value horizontalGuideCountBefore || true) -> $(status_value horizontalGuideCountAfterCreate || true) -> $(status_value horizontalGuideCountAfterHide || true) -> $(status_value horizontalGuideCountFinal || true)"
          echo "Vertical hide variant: $(status_value verticalGuideCountBefore || true) -> $(status_value verticalGuideCountAfterCreate || true) -> $(status_value verticalGuideCountAfterHide || true) -> $(status_value verticalGuideCountFinal || true)"
          echo "Hide endpoints: H $(status_value horizontalHideEnd || true), V $(status_value verticalHideEnd || true)"
          echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
          echo "Guide pixels: changed neutral pixels $(status_value changedNeutralPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-ruler-guide-lines" ]]; then
          echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
          echo "Guide line positions: H $(status_value firstHGuides || true) -> $(status_value secondHGuides || true), V $(status_value firstVGuides || true) -> $(status_value secondVGuides || true)"
          echo "First guide lines: neutral $(status_value firstGuideLineChangedNeutralPixels || true), columns $(status_value firstVerticalGuideLineColumns || true), rows $(status_value firstHorizontalGuideLineRows || true), dash V/H $(status_value firstVerticalGuideLineDashSegments || true)/$(status_value firstHorizontalGuideLineDashSegments || true)"
          echo "Second guide lines: neutral $(status_value secondGuideLineChangedNeutralPixels || true), columns $(status_value secondVerticalGuideLineColumns || true), rows $(status_value secondHorizontalGuideLineRows || true), dash V/H $(status_value secondVerticalGuideLineDashSegments || true)/$(status_value secondHorizontalGuideLineDashSegments || true)"
          echo "Guide line bands: first V/H $(status_value firstVerticalGuideBandPixels || true)/$(status_value firstHorizontalGuideBandPixels || true), second V/H $(status_value secondVerticalGuideBandPixels || true)/$(status_value secondHorizontalGuideBandPixels || true)"
          echo "Moved guide lines: neutral $(status_value movedGuideLineChangedNeutralPixels || true), columns $(status_value movedVerticalGuideLineColumns || true), rows $(status_value movedHorizontalGuideLineRows || true)"
        fi
        if [[ "$smoke_action" == "viewer-ruler-guide-styles" ]]; then
          echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
          echo "Ruler style colors: background $(status_value rulerStyleBackgroundColor || true), scale $(status_value rulerStyleScaleColor || true), handle $(status_value rulerStyleHandleColor || true)"
          echo "Horizontal ruler style pixels: background $(status_value horizontalRulerStyleBackgroundPixels || true), handle $(status_value horizontalRulerStyleHandlePixels || true), scale $(status_value horizontalRulerStyleScalePixels || true), changed $(status_value horizontalRulerChangedPixels || true)"
          echo "Vertical ruler style pixels: background $(status_value verticalRulerStyleBackgroundPixels || true), handle $(status_value verticalRulerStyleHandlePixels || true), scale $(status_value verticalRulerStyleScalePixels || true), changed $(status_value verticalRulerChangedPixels || true)"
          echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
          echo "Guide pixels: changed neutral pixels $(status_value changedNeutralPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-ruler-ticks" ]]; then
          echo "Ruler overlays: guide disabled $(status_value viewGuideDisabled || true), ruler enabled $(status_value viewRulerEnabled || true)"
          echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
          echo "Horizontal ruler ticks: $(status_value horizontalRulerTickPixelsBefore || true) -> $(status_value horizontalRulerTickPixelsAfter || true), changed $(status_value horizontalRulerChangedPixels || true)"
          echo "Vertical ruler ticks: $(status_value verticalRulerTickPixelsBefore || true) -> $(status_value verticalRulerTickPixelsAfter || true), changed $(status_value verticalRulerChangedPixels || true)"
          echo "Ruler units: H $(status_value horizontalRulerUnit || true), V $(status_value verticalRulerUnit || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-overlay" || "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" || "$smoke_action" == "viewer-vector-brush" || "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
          echo "Tool: $(status_value toolName || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-overlay" || "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-animate-tool-cursors" ]]; then
          echo "Stage object: $(status_value stageObject || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" ]]; then
          echo "Stage object X: $(status_value stageObjectXBefore || true) -> $(status_value stageObjectXAfter || true)"
          echo "Stage object Y: $(status_value stageObjectYBefore || true) -> $(status_value stageObjectYAfter || true)"
          echo "Tool drag: $(status_value toolDragStart || true) -> $(status_value toolDragEnd || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-undo-redo" ]]; then
          echo "Undo X/Y: $(status_value stageObjectXUndo || true),$(status_value stageObjectYUndo || true)"
          echo "Redo X/Y: $(status_value stageObjectXRedo || true),$(status_value stageObjectYRedo || true)"
          echo "Undo history: $(status_value undoHistoryCountBefore || true) -> $(status_value undoHistoryCountAfterDrag || true), index $(status_value undoHistoryIndexBefore || true)/$(status_value undoHistoryIndexAfterDrag || true)/$(status_value undoHistoryIndexAfterUndo || true)/$(status_value undoHistoryIndexAfterRedo || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-modifiers" ]]; then
          echo "Normal delta: $(status_value normalDeltaX || true),$(status_value normalDeltaY || true)"
          echo "Alt delta: $(status_value altDeltaX || true),$(status_value altDeltaY || true)"
          echo "Shift delta: $(status_value shiftDeltaX || true),$(status_value shiftDeltaY || true)"
          echo "Cursor precise: $(status_value cursorPreciseBefore || true) -> $(status_value cursorPreciseAfter || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-handles" ]]; then
          echo "Active axis: $(status_value activeAxis || true), handle unit: $(status_value handleUnit || true)"
          echo "Rotation angle: $(status_value angleBefore || true) -> $(status_value angleAfterRotationHandle || true)"
          echo "Scale: $(status_value scaleBefore || true) -> $(status_value scaleAfterScaleHandle || true)"
          echo "Center: $(status_value centerBefore || true) -> $(status_value centerAfterCenterHandle || true)"
          echo "Hover changed pixels: $(status_value hoverChangedPixels || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-handle-variants" ]]; then
          echo "Handle variants: translation $(status_value translationBefore || true) -> $(status_value translationAfter || true)"
          echo "Handle variants: scaleXY $(status_value scaleXBefore || true),$(status_value scaleYBefore || true) -> $(status_value scaleXAfterScaleXYHandle || true),$(status_value scaleYAfterScaleXYHandle || true), shear $(status_value shearXBefore || true),$(status_value shearYBefore || true) -> $(status_value shearXAfterShearHandle || true),$(status_value shearYAfterShearHandle || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-axis-drags" ]]; then
          echo "Axis drags: position $(status_value animateAxisPositionBefore || true) -> $(status_value animateAxisPositionAfter || true), rotation $(status_value animateAxisRotationBefore || true) -> $(status_value animateAxisRotationAfter || true)"
          echo "Axis drags: scale $(status_value animateAxisScaleBefore || true) -> $(status_value animateAxisScaleAfter || true), shear $(status_value animateAxisShearBefore || true) -> $(status_value animateAxisShearAfter || true), center $(status_value animateAxisCenterBefore || true) -> $(status_value animateAxisCenterAfter || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-cursors" ]]; then
          echo "Animate/Edit cursors: position $(status_value animateCursorPositionNormalCursor || true)/$(status_value animateCursorPositionAltCursor || true), rotation $(status_value animateCursorRotationNormalCursor || true)/$(status_value animateCursorRotationAltCursor || true)"
          echo "Animate/Edit cursors: scale $(status_value animateCursorScaleNormalCursor || true)/$(status_value animateCursorScaleAltCursor || true), shear $(status_value animateCursorShearNormalCursor || true)/$(status_value animateCursorShearAltCursor || true), center $(status_value animateCursorCenterNormalCursor || true)/$(status_value animateCursorCenterAltCursor || true)"
          echo "Animate/Edit cursor artwork: position $(status_value animateCursorPositionNormalArtwork || true) / $(status_value animateCursorPositionAltArtwork || true)"
          echo "Animate/Edit cursor artwork: rotation $(status_value animateCursorRotationNormalArtwork || true) / $(status_value animateCursorRotationAltArtwork || true)"
          echo "Animate/Edit cursor artwork: scale $(status_value animateCursorScaleNormalArtwork || true) / $(status_value animateCursorScaleAltArtwork || true)"
          echo "Animate/Edit cursor artwork: shear $(status_value animateCursorShearNormalArtwork || true) / $(status_value animateCursorShearAltArtwork || true)"
          echo "Animate/Edit cursor artwork: center $(status_value animateCursorCenterNormalArtwork || true) / $(status_value animateCursorCenterAltArtwork || true)"
        fi
        if [[ "$smoke_action" == "viewer-selection-tool-vector-handles" ]]; then
          echo "Selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), count $(status_value selectionCount || true), stroke 0 selected $(status_value selectionStroke0Selected || true)"
          echo "Selection bbox: $(status_value selectionBBoxBeforeScale || true) -> $(status_value selectionBBoxAfterScale || true), delta $(status_value selectionBBoxWidthDelta || true),$(status_value selectionBBoxHeightDelta || true)"
          echo "Selection cursor: id $(status_value selectionScaleCursorId || true), artwork $(status_value selectionScaleCursorArtwork || true)"
        fi
        if [[ "$smoke_action" == "viewer-selection-tool-vector-handle-variants" ]]; then
          echo "Selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), count $(status_value selectionCount || true), stroke 0 selected $(status_value selectionStroke0Selected || true)"
          echo "Selection horizontal scale: $(status_value selectionBBoxBeforeHorizontalScale || true) -> $(status_value selectionBBoxAfterHorizontalScale || true), delta $(status_value selectionHorizontalWidthDelta || true), cursor $(status_value selectionHorizontalScaleCursorId || true)/$(status_value selectionHorizontalScaleCursorArtwork || true)"
          echo "Selection vertical scale: $(status_value selectionBBoxBeforeVerticalScale || true) -> $(status_value selectionBBoxAfterVerticalScale || true), delta $(status_value selectionVerticalHeightDelta || true), cursor $(status_value selectionVerticalScaleCursorId || true)/$(status_value selectionVerticalScaleCursorArtwork || true)"
          echo "Selection rotation: $(status_value selectionBBoxBeforeRotation || true) -> $(status_value selectionBBoxAfterRotation || true), cursor $(status_value selectionRotationCursorId || true)/$(status_value selectionRotationCursorArtwork || true)"
        fi
        if [[ "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" ]]; then
          echo "Selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), count $(status_value selectionCount || true), stroke 0 selected $(status_value selectionStroke0Selected || true)"
          echo "Selection center: $(status_value selectionCenterBefore || true) -> $(status_value selectionCenterAfter || true), delta $(status_value selectionCenterDelta || true), cursor $(status_value selectionCenterCursorId || true)/$(status_value selectionCenterCursorArtwork || true)"
          echo "Selection thickness: $(status_value selectionThicknessBefore || true) -> $(status_value selectionThicknessAfter || true), delta $(status_value selectionThicknessDelta || true), cursor $(status_value selectionThicknessCursorId || true)/$(status_value selectionThicknessCursorArtwork || true)"
          echo "Selection free deform: $(status_value selectionBBoxBeforeDeform || true) -> $(status_value selectionBBoxAfterDeform || true), cursor $(status_value selectionDeformCursorId || true)/$(status_value selectionDeformCursorArtwork || true)"
        fi
        if [[ "$smoke_action" == "viewer-selection-tool-vector-mode-variants" ]]; then
          echo "Vector selection modes: mode $(status_value selectionMode || true), freehand $(status_value selectionFreehandType || true), polyline $(status_value selectionPolylineType || true), strokes $(status_value vectorStrokeCount || true)"
          echo "Vector freehand selection: count $(status_value selectionFreehandCount || true), stroke 0 $(status_value selectionFreehandStroke0Selected || true), stroke 1 $(status_value selectionFreehandStroke1Selected || true), bbox $(status_value selectionFreehandBBox || true)"
          echo "Vector polyline selection: count $(status_value selectionPolylineCount || true), stroke 0 $(status_value selectionPolylineStroke0Selected || true), stroke 1 $(status_value selectionPolylineStroke1Selected || true), bbox $(status_value selectionPolylineBBox || true), changed from freehand $(status_value selectionBBoxChangedFromFreehand || true)"
        fi
        if [[ "$smoke_action" == "viewer-selection-tool-raster-handles" ]]; then
          echo "Raster selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), empty $(status_value selectionEmptyBefore || true) -> $(status_value selectionEmptyAfterRect || true), floating $(status_value selectionFloatingAfterRect || true) -> $(status_value selectionFloatingAfterScale || true)"
          echo "Raster selection bbox: $(status_value selectionBBoxAfterRect || true) -> $(status_value selectionBBoxAfterScale || true), delta $(status_value selectionBBoxWidthDelta || true),$(status_value selectionBBoxHeightDelta || true)"
          echo "Raster selection cursor: id $(status_value selectionScaleCursorId || true), artwork $(status_value selectionScaleCursorArtwork || true)"
        fi
        if [[ "$smoke_action" == "viewer-selection-tool-raster-mode-variants" ]]; then
          echo "Raster selection modes: mode $(status_value selectionMode || true), freehand $(status_value selectionFreehandType || true), polyline $(status_value selectionPolylineType || true)"
          echo "Raster freehand selection: empty $(status_value selectionEmptyBefore || true) -> $(status_value selectionEmptyAfterFreehand || true), floating $(status_value selectionFloatingAfterFreehand || true), bbox $(status_value selectionBBoxAfterFreehand || true)"
          echo "Raster polyline selection: empty $(status_value selectionEmptyAfterPolyline || true), floating $(status_value selectionFloatingAfterPolyline || true), bbox $(status_value selectionBBoxAfterPolyline || true), changed from freehand $(status_value selectionBBoxChangedFromFreehand || true)"
        fi
        if [[ "$smoke_action" == "viewer-vector-brush" ]]; then
          echo "Strokes: $(status_value strokesBefore || true) -> $(status_value strokesAfter || true)"
        fi
        if [[ "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
          echo "Raster pixels: $(status_value rasterPixelsBefore || true) -> $(status_value rasterPixelsAfter || true)"
          echo "Raster red pixels: $(status_value rasterRedPixelsBefore || true) -> $(status_value rasterRedPixelsAfter || true)"
        fi
        if [[ "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-animate-tool-cursors" || "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
          echo "Input path: $(status_value toolInputPath || true)"
        fi
        if [[ "$smoke_action" == "viewer-raster-brush-tablet-events" ]]; then
          echo "Tablet pressure: $(status_value tabletPressurePress || true) -> $(status_value tabletPressureDrag || true)"
          echo "Tablet tilt: $(status_value tabletTiltX || true),$(status_value tabletTiltY || true)"
        fi
        echo "Framebuffer: ${framebuffer_width}x${framebuffer_height}"
        echo "Changed pixels: $changed_pixels"
        echo "Red pixels: $red_pixels"
        if [[ "$smoke_action" == "viewer-render" ]]; then
          echo "Second-frame changed pixels: $stale_frame_changed_pixels"
          echo "Second-frame green pixels: $stale_frame_green_pixels"
          echo "Second-frame changed green pixels: $stale_frame_changed_green_pixels"
          echo "Second-frame capture: $stale_frame_capture_path"
        fi
        echo "Before capture: $before_capture_path"
        echo "After capture: $after_capture_path"
        echo "Log: $log_path"
        wait "$pid"
        exit $?
      fi

      stop_smoke_process
      echo "$smoke_label ${smoke_action} smoke passed: $viewer_window"
      echo "Content: $viewer_content"
      echo "Viewer logical size: ${viewer_logical_width}x${viewer_logical_height}"
      echo "Viewer DPR: ${viewer_dev_pix_ratio} (${viewer_device_pixel_ratio}; screen ${viewer_screen_device_pixel_ratio})"
      if [[ "$smoke_action" == "viewer-zoom-pan" ]]; then
        echo "Transform: zoom $(status_value viewerZoomFactor || true), pan $(status_value viewerPanLogicalX || true),$(status_value viewerPanLogicalY || true) logical pixels"
        echo "Viewer scale: $(status_value viewerScaleBefore || true) -> $(status_value viewerScaleAfter || true)"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin" ]]; then
        echo "Onion skin: row $(status_value onionSkinCurrentRow || true), back offset $(status_value onionSkinBackOffset || true), mobile count $(status_value onionSkinMosCount || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-rowarea" ]]; then
        echo "Onion row area: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true)"
        echo "Onion toggles: markers $(status_value onionSkinEnabledAfterMarkers || true), disabled $(status_value onionSkinEnabledAfterDisable || true), enabled $(status_value onionSkinEnabledAfterEnable || true)"
        echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-rowarea-drag" ]]; then
        echo "Onion row area drag: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true), range $(status_value onionSkinMosRange || true)"
        echo "Row area drag: delivered $(status_value rowAreaDragEventDelivered || true), points $(status_value rowAreaDragPoints || true)"
        echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-fixed-marker-drag" ]]; then
        echo "Fixed onion marker drag: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true)"
        echo "Fixed onion marker counts: add $(status_value onionSkinFosCountAfterAddBack || true), remove $(status_value onionSkinFosCountAfterRemoveBack || true), final $(status_value onionSkinFosCount || true)"
        echo "Fixed onion marker events: add $(status_value rowAreaAddFixedDragDelivered || true), remove $(status_value rowAreaRemoveFixedDragDelivered || true), front $(status_value rowAreaAddFrontFixedDragDelivered || true)"
        echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-shift-trace" ]]; then
        echo "Shift trace offsets: initial $(status_value shiftTraceInitialOffsets || true), moved $(status_value shiftTraceAfterMoveFrontOffsets || true), reset $(status_value shiftTraceAfterResetOffsets || true), hidden $(status_value shiftTraceAfterHideFrontOffsets || true), final $(status_value shiftTraceFinalOffsets || true)"
        echo "Shift trace events: move back $(status_value shiftTraceMoveBackEventDelivered || true), move front $(status_value shiftTraceMoveFrontEventDelivered || true), reset $(status_value shiftTraceResetEventDelivered || true), hide back $(status_value shiftTraceHideBackEventDelivered || true), hide front $(status_value shiftTraceHideFrontEventDelivered || true)"
        echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-context-menu" ]]; then
        echo "Onion context menu: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true), range $(status_value onionSkinMosRange || true)"
        echo "Onion commands: activate $(status_value onionSkinActivateCommand || true), deactivate $(status_value onionSkinDeactivateCommand || true), extend $(status_value onionSkinExtendCommand || true), limit $(status_value onionSkinLimitCommand || true), clear fixed $(status_value onionSkinClearFixedCommand || true), clear relative $(status_value onionSkinClearRelativeCommand || true), clear all $(status_value onionSkinClearAllCommand || true)"
        echo "Row area pixels: changed $(status_value xsheetRowAreaChangedPixels || true), non-background $(status_value xsheetRowAreaNonBackgroundPixels || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-custom-colors" ]]; then
        echo "Onion custom colors: back $(status_value onionSkinBackColor || true), front $(status_value onionSkinFrontColor || true)"
        echo "Onion color pixels: viewer back $(status_value viewerBackColorPixels || true), viewer front $(status_value viewerFrontColorPixels || true), row back $(status_value xsheetRowAreaBackColorPixels || true), row front $(status_value xsheetRowAreaFrontColorPixels || true)"
        echo "Onion rows: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), rows $(status_value onionSkinRows || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-onion-skin-orientations" ]]; then
        echo "Onion orientations: $(status_value orientationBefore || true) -> $(status_value orientationAfter || true)"
        echo "Vertical row area pixels: changed $(status_value verticalRowAreaChangedPixels || true), non-background $(status_value verticalRowAreaNonBackgroundPixels || true)"
        echo "Horizontal row area pixels: changed $(status_value horizontalRowAreaChangedPixels || true), non-background $(status_value horizontalRowAreaNonBackgroundPixels || true)"
        echo "Onion rows: row $(status_value onionSkinCurrentRow || true), MOS $(status_value onionSkinMosCount || true), FOS $(status_value onionSkinFosCount || true), rows $(status_value onionSkinRows || true)"
        echo "Onion pixels: ${baseline_onion_pixels} -> ${onion_pixels}"
      fi
      if [[ "$smoke_action" == "viewer-camera-overlay" ]]; then
        echo "Camera overlay: disabled $(status_value cameraOverlayDisabled || true), enabled $(status_value cameraOverlayEnabled || true)"
      fi
      if [[ "$smoke_action" == "viewer-safe-area-field-guide" ]]; then
        echo "Safe area: disabled $(status_value safeAreaDisabled || true), enabled $(status_value safeAreaEnabled || true)"
        echo "Field guide: disabled $(status_value fieldGuideDisabled || true), enabled $(status_value fieldGuideEnabled || true)"
        echo "Gray pixels: $(status_value grayPixels || true), changed gray pixels: $(status_value changedGrayPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-safe-area-presets" ]]; then
        echo "Safe area preset: $(status_value safeAreaNameDefault || true) -> $(status_value safeAreaNameCustom || true), stored $(status_value storedSafeAreaName || true)"
        echo "Default safe area pixels: red $(status_value defaultSafeAreaRedPixels || true), changed red $(status_value defaultSafeAreaChangedRedPixels || true)"
        echo "Custom safe area pixels: red $(status_value customSafeAreaRedPixels || true), green $(status_value customSafeAreaGreenPixels || true), blue $(status_value customSafeAreaBluePixels || true)"
        echo "Safe area preset delta: changed $(status_value safeAreaPresetChangedPixels || true), green $(status_value safeAreaPresetChangedGreenPixels || true), blue $(status_value safeAreaPresetChangedBluePixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-safe-area-custom-file" ]]; then
        echo "Custom safe area file: written $(status_value customSafeAreaFileWritten || true), restored $(status_value safeAreaCustomFileRestored || true)"
        echo "Custom safe area preset: $(status_value customSafeAreaName || true), stored $(status_value storedSafeAreaName || true)"
        echo "Custom safe area pixels: red $(status_value customSafeAreaRedPixels || true), green $(status_value customSafeAreaGreenPixels || true), blue $(status_value customSafeAreaBluePixels || true)"
        echo "Custom safe area changed pixels: red $(status_value customSafeAreaChangedRedPixels || true), green $(status_value customSafeAreaChangedGreenPixels || true), blue $(status_value customSafeAreaChangedBluePixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-field-guide-settings" ]]; then
        echo "Field guide overlay: disabled $(status_value fieldGuideDisabled || true), enabled $(status_value fieldGuideEnabled || true)"
        echo "Field guide settings: size $(status_value initialFieldGuideSize || true) -> $(status_value firstFieldGuideSize || true) -> $(status_value secondFieldGuideSize || true), aspect $(status_value initialFieldGuideAspectRatio || true) -> $(status_value firstFieldGuideAspectRatio || true) -> $(status_value secondFieldGuideAspectRatio || true)"
        echo "Field guide pixels: first gray $(status_value firstFieldGuideGrayPixels || true), changed $(status_value firstFieldGuideChangedGrayPixels || true); second gray $(status_value secondFieldGuideGrayPixels || true), changed $(status_value secondFieldGuideChangedGrayPixels || true)"
        echo "Field guide settings delta: changed $(status_value fieldGuideSettingsChangedPixels || true), gray $(status_value fieldGuideSettingsChangedGrayPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-ruler-guide" ]]; then
        echo "Guide overlay: disabled $(status_value viewGuideDisabled || true), enabled $(status_value viewGuideEnabled || true)"
        echo "Ruler overlay: disabled $(status_value viewRulerDisabled || true), enabled $(status_value viewRulerEnabled || true)"
        echo "Guides: H $(status_value hGuideCount || true), V $(status_value vGuideCount || true)"
        echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
        echo "Gray pixels: $(status_value grayPixels || true), changed gray pixels: $(status_value changedGrayPixels || true), changed neutral pixels: $(status_value changedNeutralPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-ruler-guide-events" ]]; then
        echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
        echo "Horizontal guides: $(status_value horizontalGuideCountBefore || true) -> $(status_value horizontalGuideCountAfterMove || true) -> $(status_value horizontalGuideCountAfterDelete || true)"
        echo "Vertical guides: $(status_value verticalGuideCountBefore || true) -> $(status_value verticalGuideCountAfterMove || true) -> $(status_value verticalGuideCountAfterDelete || true)"
        echo "Guide values: H $(status_value horizontalGuideStart || true) -> $(status_value horizontalGuideAfterMove || true) -> deleted, V $(status_value verticalGuideStart || true) -> $(status_value verticalGuideAfterMove || true)"
        echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
        echo "Gray pixels: $(status_value grayPixels || true), changed gray pixels: $(status_value changedGrayPixels || true), changed neutral pixels: $(status_value changedNeutralPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-ruler-guide-variants" ]]; then
        echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
        echo "Horizontal hide variant: $(status_value horizontalGuideCountBefore || true) -> $(status_value horizontalGuideCountAfterCreate || true) -> $(status_value horizontalGuideCountAfterHide || true) -> $(status_value horizontalGuideCountFinal || true)"
        echo "Vertical hide variant: $(status_value verticalGuideCountBefore || true) -> $(status_value verticalGuideCountAfterCreate || true) -> $(status_value verticalGuideCountAfterHide || true) -> $(status_value verticalGuideCountFinal || true)"
        echo "Hide endpoints: H $(status_value horizontalHideEnd || true), V $(status_value verticalHideEnd || true)"
        echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
        echo "Guide pixels: changed neutral pixels $(status_value changedNeutralPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-ruler-guide-lines" ]]; then
        echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
        echo "Guide line positions: H $(status_value firstHGuides || true) -> $(status_value secondHGuides || true), V $(status_value firstVGuides || true) -> $(status_value secondVGuides || true)"
        echo "First guide lines: neutral $(status_value firstGuideLineChangedNeutralPixels || true), columns $(status_value firstVerticalGuideLineColumns || true), rows $(status_value firstHorizontalGuideLineRows || true), dash V/H $(status_value firstVerticalGuideLineDashSegments || true)/$(status_value firstHorizontalGuideLineDashSegments || true)"
        echo "Second guide lines: neutral $(status_value secondGuideLineChangedNeutralPixels || true), columns $(status_value secondVerticalGuideLineColumns || true), rows $(status_value secondHorizontalGuideLineRows || true), dash V/H $(status_value secondVerticalGuideLineDashSegments || true)/$(status_value secondHorizontalGuideLineDashSegments || true)"
        echo "Guide line bands: first V/H $(status_value firstVerticalGuideBandPixels || true)/$(status_value firstHorizontalGuideBandPixels || true), second V/H $(status_value secondVerticalGuideBandPixels || true)/$(status_value secondHorizontalGuideBandPixels || true)"
        echo "Moved guide lines: neutral $(status_value movedGuideLineChangedNeutralPixels || true), columns $(status_value movedVerticalGuideLineColumns || true), rows $(status_value movedHorizontalGuideLineRows || true)"
      fi
      if [[ "$smoke_action" == "viewer-ruler-guide-styles" ]]; then
        echo "Guide/ruler overlays: guide $(status_value viewGuideEnabled || true), ruler $(status_value viewRulerEnabled || true)"
        echo "Ruler style colors: background $(status_value rulerStyleBackgroundColor || true), scale $(status_value rulerStyleScaleColor || true), handle $(status_value rulerStyleHandleColor || true)"
        echo "Horizontal ruler style pixels: background $(status_value horizontalRulerStyleBackgroundPixels || true), handle $(status_value horizontalRulerStyleHandlePixels || true), scale $(status_value horizontalRulerStyleScalePixels || true), changed $(status_value horizontalRulerChangedPixels || true)"
        echo "Vertical ruler style pixels: background $(status_value verticalRulerStyleBackgroundPixels || true), handle $(status_value verticalRulerStyleHandlePixels || true), scale $(status_value verticalRulerStyleScalePixels || true), changed $(status_value verticalRulerChangedPixels || true)"
        echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
        echo "Guide pixels: changed neutral pixels $(status_value changedNeutralPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-ruler-ticks" ]]; then
        echo "Ruler overlays: guide disabled $(status_value viewGuideDisabled || true), ruler enabled $(status_value viewRulerEnabled || true)"
        echo "Ruler widgets: $(status_value visibleRulerWidgetCount || true) visible of $(status_value rulerWidgetCount || true)"
        echo "Horizontal ruler ticks: $(status_value horizontalRulerTickPixelsBefore || true) -> $(status_value horizontalRulerTickPixelsAfter || true), changed $(status_value horizontalRulerChangedPixels || true)"
        echo "Vertical ruler ticks: $(status_value verticalRulerTickPixelsBefore || true) -> $(status_value verticalRulerTickPixelsAfter || true), changed $(status_value verticalRulerChangedPixels || true)"
        echo "Ruler units: H $(status_value horizontalRulerUnit || true), V $(status_value verticalRulerUnit || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-overlay" || "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" || "$smoke_action" == "viewer-vector-brush" || "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        echo "Tool: $(status_value toolName || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-overlay" || "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-animate-tool-cursors" ]]; then
        echo "Stage object: $(status_value stageObject || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-drag" || "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" ]]; then
        echo "Stage object X: $(status_value stageObjectXBefore || true) -> $(status_value stageObjectXAfter || true)"
        echo "Stage object Y: $(status_value stageObjectYBefore || true) -> $(status_value stageObjectYAfter || true)"
        echo "Tool drag: $(status_value toolDragStart || true) -> $(status_value toolDragEnd || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-undo-redo" ]]; then
        echo "Undo X/Y: $(status_value stageObjectXUndo || true),$(status_value stageObjectYUndo || true)"
        echo "Redo X/Y: $(status_value stageObjectXRedo || true),$(status_value stageObjectYRedo || true)"
        echo "Undo history: $(status_value undoHistoryCountBefore || true) -> $(status_value undoHistoryCountAfterDrag || true), index $(status_value undoHistoryIndexBefore || true)/$(status_value undoHistoryIndexAfterDrag || true)/$(status_value undoHistoryIndexAfterUndo || true)/$(status_value undoHistoryIndexAfterRedo || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-modifiers" ]]; then
        echo "Normal delta: $(status_value normalDeltaX || true),$(status_value normalDeltaY || true)"
        echo "Alt delta: $(status_value altDeltaX || true),$(status_value altDeltaY || true)"
        echo "Shift delta: $(status_value shiftDeltaX || true),$(status_value shiftDeltaY || true)"
        echo "Cursor precise: $(status_value cursorPreciseBefore || true) -> $(status_value cursorPreciseAfter || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-handles" ]]; then
        echo "Active axis: $(status_value activeAxis || true), handle unit: $(status_value handleUnit || true)"
        echo "Rotation angle: $(status_value angleBefore || true) -> $(status_value angleAfterRotationHandle || true)"
        echo "Scale: $(status_value scaleBefore || true) -> $(status_value scaleAfterScaleHandle || true)"
        echo "Center: $(status_value centerBefore || true) -> $(status_value centerAfterCenterHandle || true)"
        echo "Hover changed pixels: $(status_value hoverChangedPixels || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-handle-variants" ]]; then
        echo "Handle variants: translation $(status_value translationBefore || true) -> $(status_value translationAfter || true)"
        echo "Handle variants: scaleXY $(status_value scaleXBefore || true),$(status_value scaleYBefore || true) -> $(status_value scaleXAfterScaleXYHandle || true),$(status_value scaleYAfterScaleXYHandle || true), shear $(status_value shearXBefore || true),$(status_value shearYBefore || true) -> $(status_value shearXAfterShearHandle || true),$(status_value shearYAfterShearHandle || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-axis-drags" ]]; then
        echo "Axis drags: position $(status_value animateAxisPositionBefore || true) -> $(status_value animateAxisPositionAfter || true), rotation $(status_value animateAxisRotationBefore || true) -> $(status_value animateAxisRotationAfter || true)"
        echo "Axis drags: scale $(status_value animateAxisScaleBefore || true) -> $(status_value animateAxisScaleAfter || true), shear $(status_value animateAxisShearBefore || true) -> $(status_value animateAxisShearAfter || true), center $(status_value animateAxisCenterBefore || true) -> $(status_value animateAxisCenterAfter || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-cursors" ]]; then
        echo "Animate/Edit cursors: position $(status_value animateCursorPositionNormalCursor || true)/$(status_value animateCursorPositionAltCursor || true), rotation $(status_value animateCursorRotationNormalCursor || true)/$(status_value animateCursorRotationAltCursor || true)"
        echo "Animate/Edit cursors: scale $(status_value animateCursorScaleNormalCursor || true)/$(status_value animateCursorScaleAltCursor || true), shear $(status_value animateCursorShearNormalCursor || true)/$(status_value animateCursorShearAltCursor || true), center $(status_value animateCursorCenterNormalCursor || true)/$(status_value animateCursorCenterAltCursor || true)"
        echo "Animate/Edit cursor artwork: position $(status_value animateCursorPositionNormalArtwork || true) / $(status_value animateCursorPositionAltArtwork || true)"
        echo "Animate/Edit cursor artwork: rotation $(status_value animateCursorRotationNormalArtwork || true) / $(status_value animateCursorRotationAltArtwork || true)"
        echo "Animate/Edit cursor artwork: scale $(status_value animateCursorScaleNormalArtwork || true) / $(status_value animateCursorScaleAltArtwork || true)"
        echo "Animate/Edit cursor artwork: shear $(status_value animateCursorShearNormalArtwork || true) / $(status_value animateCursorShearAltArtwork || true)"
        echo "Animate/Edit cursor artwork: center $(status_value animateCursorCenterNormalArtwork || true) / $(status_value animateCursorCenterAltArtwork || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-handles" ]]; then
        echo "Selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), count $(status_value selectionCount || true), stroke 0 selected $(status_value selectionStroke0Selected || true)"
        echo "Selection bbox: $(status_value selectionBBoxBeforeScale || true) -> $(status_value selectionBBoxAfterScale || true), delta $(status_value selectionBBoxWidthDelta || true),$(status_value selectionBBoxHeightDelta || true)"
        echo "Selection cursor: id $(status_value selectionScaleCursorId || true), artwork $(status_value selectionScaleCursorArtwork || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-handle-variants" ]]; then
        echo "Selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), count $(status_value selectionCount || true), stroke 0 selected $(status_value selectionStroke0Selected || true)"
        echo "Selection horizontal scale: $(status_value selectionBBoxBeforeHorizontalScale || true) -> $(status_value selectionBBoxAfterHorizontalScale || true), delta $(status_value selectionHorizontalWidthDelta || true), cursor $(status_value selectionHorizontalScaleCursorId || true)/$(status_value selectionHorizontalScaleCursorArtwork || true)"
        echo "Selection vertical scale: $(status_value selectionBBoxBeforeVerticalScale || true) -> $(status_value selectionBBoxAfterVerticalScale || true), delta $(status_value selectionVerticalHeightDelta || true), cursor $(status_value selectionVerticalScaleCursorId || true)/$(status_value selectionVerticalScaleCursorArtwork || true)"
        echo "Selection rotation: $(status_value selectionBBoxBeforeRotation || true) -> $(status_value selectionBBoxAfterRotation || true), cursor $(status_value selectionRotationCursorId || true)/$(status_value selectionRotationCursorArtwork || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" ]]; then
        echo "Selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), count $(status_value selectionCount || true), stroke 0 selected $(status_value selectionStroke0Selected || true)"
        echo "Selection center: $(status_value selectionCenterBefore || true) -> $(status_value selectionCenterAfter || true), delta $(status_value selectionCenterDelta || true), cursor $(status_value selectionCenterCursorId || true)/$(status_value selectionCenterCursorArtwork || true)"
        echo "Selection thickness: $(status_value selectionThicknessBefore || true) -> $(status_value selectionThicknessAfter || true), delta $(status_value selectionThicknessDelta || true), cursor $(status_value selectionThicknessCursorId || true)/$(status_value selectionThicknessCursorArtwork || true)"
        echo "Selection free deform: $(status_value selectionBBoxBeforeDeform || true) -> $(status_value selectionBBoxAfterDeform || true), cursor $(status_value selectionDeformCursorId || true)/$(status_value selectionDeformCursorArtwork || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-vector-mode-variants" ]]; then
        echo "Vector selection modes: mode $(status_value selectionMode || true), freehand $(status_value selectionFreehandType || true), polyline $(status_value selectionPolylineType || true), strokes $(status_value vectorStrokeCount || true)"
        echo "Vector freehand selection: count $(status_value selectionFreehandCount || true), stroke 0 $(status_value selectionFreehandStroke0Selected || true), stroke 1 $(status_value selectionFreehandStroke1Selected || true), bbox $(status_value selectionFreehandBBox || true)"
        echo "Vector polyline selection: count $(status_value selectionPolylineCount || true), stroke 0 $(status_value selectionPolylineStroke0Selected || true), stroke 1 $(status_value selectionPolylineStroke1Selected || true), bbox $(status_value selectionPolylineBBox || true), changed from freehand $(status_value selectionBBoxChangedFromFreehand || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-raster-handles" ]]; then
        echo "Raster selection tool: mode $(status_value selectionMode || true), type $(status_value selectionType || true), empty $(status_value selectionEmptyBefore || true) -> $(status_value selectionEmptyAfterRect || true), floating $(status_value selectionFloatingAfterRect || true) -> $(status_value selectionFloatingAfterScale || true)"
        echo "Raster selection bbox: $(status_value selectionBBoxAfterRect || true) -> $(status_value selectionBBoxAfterScale || true), delta $(status_value selectionBBoxWidthDelta || true),$(status_value selectionBBoxHeightDelta || true)"
        echo "Raster selection cursor: id $(status_value selectionScaleCursorId || true), artwork $(status_value selectionScaleCursorArtwork || true)"
      fi
      if [[ "$smoke_action" == "viewer-selection-tool-raster-mode-variants" ]]; then
        echo "Raster selection modes: mode $(status_value selectionMode || true), freehand $(status_value selectionFreehandType || true), polyline $(status_value selectionPolylineType || true)"
        echo "Raster freehand selection: empty $(status_value selectionEmptyBefore || true) -> $(status_value selectionEmptyAfterFreehand || true), floating $(status_value selectionFloatingAfterFreehand || true), bbox $(status_value selectionBBoxAfterFreehand || true)"
        echo "Raster polyline selection: empty $(status_value selectionEmptyAfterPolyline || true), floating $(status_value selectionFloatingAfterPolyline || true), bbox $(status_value selectionBBoxAfterPolyline || true), changed from freehand $(status_value selectionBBoxChangedFromFreehand || true)"
      fi
      if [[ "$smoke_action" == "viewer-vector-brush" ]]; then
        echo "Strokes: $(status_value strokesBefore || true) -> $(status_value strokesAfter || true)"
      fi
      if [[ "$smoke_action" == "viewer-raster-brush" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        echo "Raster pixels: $(status_value rasterPixelsBefore || true) -> $(status_value rasterPixelsAfter || true)"
        echo "Raster red pixels: $(status_value rasterRedPixelsBefore || true) -> $(status_value rasterRedPixelsAfter || true)"
      fi
      if [[ "$smoke_action" == "viewer-animate-tool-mouse-events" || "$smoke_action" == "viewer-animate-tool-undo-redo" || "$smoke_action" == "viewer-animate-tool-modifiers" || "$smoke_action" == "viewer-animate-tool-handles" || "$smoke_action" == "viewer-animate-tool-handle-variants" || "$smoke_action" == "viewer-animate-tool-axis-drags" || "$smoke_action" == "viewer-animate-tool-cursors" || "$smoke_action" == "viewer-selection-tool-vector-handles" || "$smoke_action" == "viewer-selection-tool-vector-handle-variants" || "$smoke_action" == "viewer-selection-tool-vector-center-thickness-deform" || "$smoke_action" == "viewer-selection-tool-vector-mode-variants" || "$smoke_action" == "viewer-selection-tool-raster-handles" || "$smoke_action" == "viewer-selection-tool-raster-mode-variants" || "$smoke_action" == "viewer-raster-brush-mouse-events" || "$smoke_action" == "viewer-raster-brush-tablet-events" || "$smoke_action" == "viewer-raster-brush-system-events" ]]; then
        echo "Input path: $(status_value toolInputPath || true)"
      fi
      if [[ "$smoke_action" == "viewer-raster-brush-tablet-events" ]]; then
        echo "Tablet pressure: $(status_value tabletPressurePress || true) -> $(status_value tabletPressureDrag || true)"
        echo "Tablet tilt: $(status_value tabletTiltX || true),$(status_value tabletTiltY || true)"
      fi
      echo "Framebuffer: ${framebuffer_width}x${framebuffer_height}"
      echo "Changed pixels: $changed_pixels"
      echo "Red pixels: $red_pixels"
      if [[ "$smoke_action" == "viewer-render" ]]; then
        echo "Second-frame changed pixels: $stale_frame_changed_pixels"
        echo "Second-frame green pixels: $stale_frame_green_pixels"
        echo "Second-frame changed green pixels: $stale_frame_changed_green_pixels"
        echo "Second-frame capture: $stale_frame_capture_path"
      fi
      echo "After capture: $after_capture_path"
      echo "Log: $log_path"
      exit 0
    fi

    if [[ "$hold_app" == "1" ]]; then
      echo "$smoke_label smoke launched and is still running after ${stable_seconds}s: pid $pid"
      echo "Log: $log_path"
      wait_for_smoke_process_exit
      exit $?
    fi

    stop_smoke_process
    echo "$smoke_label smoke passed: app stayed running for ${stable_seconds}s"
    echo "Log: $log_path"
    exit 0
  fi

  if ((elapsed >= timeout_seconds)); then
    stop_smoke_process
    echo "error: $smoke_label smoke timed out after ${timeout_seconds}s" >&2
    sed -n '1,180p' "$log_path" >&2
    exit 124
  fi

  sleep 1
  elapsed=$((elapsed + 1))
done

set +e
wait_for_smoke_process_exit
status=$?
set -e

echo "error: $smoke_label smoke exited early with status $status" >&2
sed -n '1,180p' "$log_path" >&2
exit "$status"
